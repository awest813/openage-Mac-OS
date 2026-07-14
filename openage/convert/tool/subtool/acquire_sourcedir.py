# Copyright 2020-2026 the openage authors. See copying.md for legal info.
#
# pylint: disable=too-many-branches

"""
Acquire the sourcedir for the game that is supposed to be converted.
"""
from __future__ import annotations
import platform
import re
import typing

from configparser import ConfigParser
import os
from pathlib import Path
import subprocess
import sys
from typing import AnyStr, Generator

import shutil
import tempfile
from urllib.request import urlopen

from ....log import warn, info, dbg
from ....util.fslike.directory import CaseIgnoringDirectory, Directory

if typing.TYPE_CHECKING:
    from openage.convert.value_object.init.game_version import GameEdition


STANDARD_PATH_IN_32BIT_WINEPREFIX =\
    "drive_c/Program Files/Microsoft Games/Age of Empires II/"
STANDARD_PATH_IN_64BIT_WINEPREFIX =\
    "drive_c/Program Files (x86)/Microsoft Games/Age of Empires II/"
STANDARD_PATH_IN_WINEPREFIX_STEAM = \
    "drive_c/Program Files (x86)/Steam/steamapps/common/Age2HD/"
REGISTRY_KEY = \
    "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Microsoft Games\\"
REGISTRY_SUFFIX_AOK = "Age of Empires\\2.0"
REGISTRY_SUFFIX_TC = "Age of Empires II: The Conquerors Expansion\\1.0"

TRIAL_URL = 'https://archive.org/download/AgeOfEmpiresIiTheConquerorsDemo/Age2XTrial.exe'

# Env vars that may point at a game install root (checked in order).
SOURCE_DIR_ENV_VARS = ("OPENAGE_SOURCE_DIR", "AGE2DIR")

STEAM_COMMON_GAME_DIRS = (
    "AoEDE",
    "Age2HD",
    "AoE2DE",
    "STAR WARS - Galactic Battlegrounds Saga",
)

# Sentinel input tokens that open a native folder picker.
BROWSE_TOKENS = frozenset({"b", "browse", "finder", "open", "pick"})


def expand_relative_path(path: str) -> AnyStr:
    """Expand relative path to an absolute one, including abbreviations like
    ~ and environment variables"""
    return os.path.realpath(os.path.expandvars(os.path.expanduser(path)))


def resolve_source_dir_override(
    cli_source_dir: typing.Union[str, None] = None,
) -> typing.Union[str, None]:
    """
    Resolve an explicit conversion source directory from CLI or environment.

    Precedence: CLI value, then OPENAGE_SOURCE_DIR, then AGE2DIR.
    Returns an expanded absolute path string, or None if unset.
    """
    candidates = []
    if cli_source_dir:
        candidates.append(cli_source_dir)
    for env_name in SOURCE_DIR_ENV_VARS:
        env_val = os.environ.get(env_name)
        if env_val:
            candidates.append(env_val)

    for candidate in candidates:
        expanded = expand_relative_path(candidate)
        if Path(expanded).is_dir():
            return expanded
        warn("Ignoring invalid source directory override: %s", candidate)

    return None


def prompt(msg: str, answer: typing.Union[bool, None] = None) -> bool:
    """
    Ask the user a yes/no question.

    :param msg: Message to display.
    :param answer: Pre-determined answer (optional).
    """
    while answer is None:
        print(f"  {msg} [Y/n]")

        user_selection = input("> ")
        if user_selection.lower() in {"yes", "y", ""}:
            answer = True

        elif user_selection.lower() in {"no", "n"}:
            answer = False

    return answer


def wanna_convert(answer: typing.Union[bool, None] = None) -> bool:
    """
    Ask the user if assets should be converted.
    """
    return prompt("Do you want to convert assets?", answer=answer)


def wanna_check_updates(answer: typing.Union[bool, None] = None) -> bool:
    """
    Ask the user if they want to check for updates.
    """
    return prompt("Do you want to check for updates?", answer=answer)


def wanna_download_trial(answer: typing.Union[bool, None] = None) -> bool:
    """
    Ask the user if the AoC trial should be downloaded.
    """
    return prompt("Do you want to download the AoC trial version?", answer=answer)


def pick_directory_native(
    prompt_text: str = "Select your Age of Empires installation folder",
) -> typing.Union[str, None]:
    """
    Open a native folder picker and return the chosen path, or None if cancelled.

    On macOS this uses Finder via osascript (no extra dependencies). On Linux it
    tries zenity or kdialog when available. Returns None on unsupported platforms
    or if the user cancels.
    """
    system = platform.system()

    if system == "Darwin":
        script = (
            f'POSIX path of (choose folder with prompt "{prompt_text}")'
        )
        try:
            result = subprocess.run(
                ["osascript", "-e", script],
                check=False,
                capture_output=True,
                text=True,
                timeout=600,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            dbg("macOS folder picker failed: %s", error)
            return None

        if result.returncode != 0:
            dbg("macOS folder picker cancelled or failed: %s", result.stderr.strip())
            return None

        chosen = result.stdout.strip().rstrip("/")
        return chosen or None

    if system == "Linux":
        for argv in (
            ["zenity", "--file-selection", "--directory", f"--title={prompt_text}"],
            ["kdialog", "--getexistingdirectory", str(Path.home()), prompt_text],
        ):
            try:
                result = subprocess.run(
                    argv,
                    check=False,
                    capture_output=True,
                    text=True,
                    timeout=600,
                )
            except (OSError, subprocess.TimeoutExpired):
                continue
            if result.returncode == 0:
                chosen = result.stdout.strip()
                if chosen:
                    return chosen
        return None

    return None


def print_macos_import_help() -> None:
    """Print short guidance when automatic discovery finds nothing on macOS."""
    print()
    print("macOS tips for importing game files:")
    print("  • Steam DE/HD: ~/Library/Application Support/Steam/steamapps/common/")
    print("  • Secondary Steam libraries are read from libraryfolders.vdf")
    print("  • Classic AoC/AOK: install under Wine or CrossOver, then browse to")
    print("    drive_c/Program Files (x86)/Microsoft Games/Age of Empires II")
    print("  • Or set OPENAGE_SOURCE_DIR=/path/to/game and re-run")
    print("  • Or: ./run convert --force --source-dir /path/to/game")
    print()


def query_source_dir(proposals: set[str]) -> AnyStr:
    """
    Query interactively for a conversion source directory.

    Lists proposals and allows selection if some were found. On macOS (and
    Linux with zenity/kdialog), users can type ``browse`` / ``b`` / empty line
    to open a native folder picker. If stdin is not a TTY (e.g. double-clicked
    launcher), the native picker is offered automatically when available.
    """
    proposal_list = sorted(proposals) if proposals else []

    if proposal_list:
        print("\nPlease select an Age of Empires installation directory.")
        print("Insert the index of one of the proposals, a path, or 'browse':")
        for index, proposal in enumerate(proposal_list):
            print(f"({index}) {proposal}")
        print("(b) Browse with Finder / file dialog…")
    else:
        print("Could not find any installation directory automatically.")
        if platform.system() == "Darwin":
            print_macos_import_help()
        print("Enter an install path, or type 'browse' to pick a folder.")

    stdin_interactive = sys.stdin.isatty()

    # Non-interactive launch (Finder / .command without a TTY): jump straight
    # to the native picker when possible instead of hanging on input()/EOF.
    if not stdin_interactive:
        info("stdin is not a TTY; opening native folder picker if available")
        chosen = pick_directory_native()
        if chosen and Path(chosen).is_dir():
            return expand_relative_path(chosen)
        print("\nEOF / no folder selected, aborting")
        sys.exit(0)

    while True:
        user_selection = input("> ").strip()

        if user_selection.lower() in BROWSE_TOKENS or (
            not user_selection and not proposal_list
        ):
            chosen = pick_directory_native()
            if chosen and Path(chosen).is_dir():
                return expand_relative_path(chosen)
            if chosen:
                warn("No valid existing directory: %s", chosen)
            else:
                print("No folder selected. Enter a path, index, or 'browse'.")
            continue

        if user_selection.isdecimal() and int(user_selection) < len(proposal_list):
            sourcedir = proposal_list[int(user_selection)]
        else:
            sourcedir = user_selection

        sourcedir = expand_relative_path(sourcedir)
        if Path(sourcedir).is_dir():
            return sourcedir
        warn("No valid existing directory: %s", sourcedir)


def parse_steam_library_paths(vdf_text: str) -> list[str]:
    """
    Extract Steam library root paths from a libraryfolders.vdf body.
    """
    paths: list[str] = []
    seen: set[str] = set()

    def add(path: str) -> None:
        path = path.replace("\\\\", "\\")
        if path not in seen:
            seen.add(path)
            paths.append(path)

    for match in re.finditer(r'"path"\s*"([^"]+)"', vdf_text, flags=re.IGNORECASE):
        add(match.group(1))

    for match in re.finditer(
        r'^\s*"(\d+)"\s*"([^"]+)"',
        vdf_text,
        flags=re.MULTILINE,
    ):
        path = match.group(2)
        if "/" not in path and "\\" not in path:
            continue
        add(path)

    return paths


def steam_libraryfolders_candidates() -> list[Path]:
    """Likely libraryfolders.vdf locations for the current platform."""
    home = Path.home()
    if platform.system() == "Darwin":
        return [
            home / "Library/Application Support/Steam/steamapps/libraryfolders.vdf",
            home / "Library/Application Support/Steam/config/libraryfolders.vdf",
        ]
    if platform.system() == "Linux":
        return [
            home / ".steam/steam/steamapps/libraryfolders.vdf",
            home / ".local/share/Steam/steamapps/libraryfolders.vdf",
            home / ".steam/root/steamapps/libraryfolders.vdf",
        ]
    if platform.system() == "Windows":
        program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        return [
            Path(program_files) / "Steam/steamapps/libraryfolders.vdf",
            Path(program_files) / "Steam/config/libraryfolders.vdf",
        ]
    return []


def steam_library_roots() -> list[Path]:
    """Resolve Steam library root directories from libraryfolders.vdf files."""
    roots: list[Path] = []
    seen: set[str] = set()

    for vdf_path in steam_libraryfolders_candidates():
        if not vdf_path.is_file():
            continue
        try:
            text = vdf_path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            dbg("could not read %s: %s", vdf_path, error)
            continue

        for raw in parse_steam_library_paths(text):
            expanded = Path(expand_relative_path(raw))
            key = str(expanded)
            if key in seen:
                continue
            if expanded.is_dir():
                seen.add(key)
                roots.append(expanded)

    return roots


def steam_game_dir_proposals(
    game_folder_names: typing.Iterable[str] = STEAM_COMMON_GAME_DIRS,
) -> set[str]:
    """Propose existing steamapps/common/<game> dirs across Steam libraries."""
    proposals: set[str] = set()
    for root in steam_library_roots():
        common = root / "steamapps" / "common"
        if not common.is_dir():
            common = root / "common"
        if not common.is_dir():
            continue
        for name in game_folder_names:
            candidate = common / name
            if candidate.is_dir():
                proposals.add(str(candidate))
    return proposals


def game_folder_names_from_editions(
    avail_game_eds: list[GameEdition],
) -> set[str]:
    """Collect steamapps/common/<folder> basenames from edition install paths."""
    names: set[str] = set(STEAM_COMMON_GAME_DIRS)
    for edition in avail_game_eds:
        for platform_paths in edition.install_paths.values():
            for path in platform_paths:
                normalized = path.replace("\\", "/")
                marker = "/steamapps/common/"
                if marker not in normalized.lower():
                    continue
                idx = normalized.lower().index(marker) + len(marker)
                folder = normalized[idx:].strip("/")
                if folder:
                    names.add(folder.split("/")[0])
    return names


def acquire_conversion_source_dir(
    avail_game_eds: list[GameEdition],
    prev_srcdir_paths: set[str] = None,
    source_dir_override: typing.Union[str, None] = None,
) -> Path:
    """
    Acquires source dir for the asset conversion.

    Returns a file system-like object that holds all the required files.

    :param source_dir_override: Explicit path from CLI/env; skips interactive
                                discovery when it points at an existing directory.
    """
    override = resolve_source_dir_override(source_dir_override)
    if override:
        info("using source directory override: %s", override)
        print(f"converting from '{override}'")
        return CaseIgnoringDirectory(override).root

    try:
        proposals = set()

        # previously used source dirs
        if prev_srcdir_paths:
            for prev_srcdir_path in prev_srcdir_paths:
                if Path(prev_srcdir_path).is_dir():
                    proposals.add(prev_srcdir_path)

        # commonly used install dirs
        current_platform = platform.system()
        for game_edition in avail_game_eds:
            install_paths = game_edition.install_paths
            candidates = []
            if current_platform == 'Linux' and 'linux' in install_paths:
                candidates = install_paths["linux"]

            elif current_platform == 'Darwin' and 'macos' in install_paths:
                candidates = install_paths["macos"]

            elif current_platform == 'Windows' and 'windows' in install_paths:
                candidates = install_paths["windows"]

            else:
                continue

            for candidate in candidates:
                if Path(expand_relative_path(candidate)).is_dir():
                    proposals.add(candidate)

        # Steam secondary libraries (external drives, etc.)
        folder_names = game_folder_names_from_editions(avail_game_eds)
        proposals.update(steam_game_dir_proposals(folder_names))

        # Wine / CrossOver prefixes (classic editions on macOS)
        for wine_path in wine_srcdir_proposals():
            expanded = expand_relative_path(wine_path)
            if Path(expanded).is_dir():
                proposals.add(expanded)

        use_trial = False
        if len(proposals) == 0:
            print("\nopenage requires a local game installation for conversion")
            print("but no local installation could be found automatically.")
            # On macOS, offer Finder immediately instead of the broken trial
            # download path when a GUI picker is available.
            if platform.system() == "Darwin" and sys.stdin.isatty():
                print_macos_import_help()
                if prompt("Open Finder to choose your game folder?"):
                    chosen = pick_directory_native()
                    if chosen and Path(chosen).is_dir():
                        sourcedir = expand_relative_path(chosen)
                        print(f"converting from '{sourcedir}'")
                        return CaseIgnoringDirectory(sourcedir).root
            use_trial = wanna_download_trial()

        if use_trial:
            sourcedir = download_trial()

        else:
            sourcedir = query_source_dir(proposals)

    except KeyboardInterrupt:
        print("\nInterrupted, aborting")
        sys.exit(0)
    except EOFError:
        # Last-chance Finder pick when stdin closes (double-click launch).
        if platform.system() == "Darwin":
            chosen = pick_directory_native()
            if chosen and Path(chosen).is_dir():
                sourcedir = expand_relative_path(chosen)
                print(f"converting from '{sourcedir}'")
                return CaseIgnoringDirectory(sourcedir).root
        print("\nEOF, aborting")
        sys.exit(0)

    print(f"converting from '{sourcedir}'")

    return CaseIgnoringDirectory(sourcedir).root


def download_trial() -> AnyStr:
    """
    Download and extract the AoC trial version.

    Does not work yet. TODO: Find an exe unpack solution that works on all platforms
    """
    print(f"Downloading AoC trial version from {TRIAL_URL}")
    # pylint: disable=consider-using-with
    tempdir = tempfile.mkdtemp()
    with urlopen(TRIAL_URL) as response:
        with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
            shutil.copyfileobj(response, tmp_file)

            from ....cabextract.cab import CABFile

            cab = CABFile(tmp_file, 0x65678)

            sourcedir = Directory(tempdir).root
            print(f"Extracting game files to {sourcedir}...")
            dirs = [cab.root]

            # Loop over all files in the CAB archive and extract them
            # to the tempdir
            while len(dirs) > 0:
                cur_src_dir = dirs[0]
                cur_tgt_dir = sourcedir

                for part in cur_src_dir.parts:
                    cur_tgt_dir = cur_tgt_dir[part]
                cur_tgt_dir.mkdirs()

                dirs.remove(cur_src_dir)

                for path in cur_src_dir.iterdir():
                    if path.is_dir():
                        dirs.append(path)

                    if path.is_file():
                        with cur_tgt_dir[path.name].open("wb") as target_file:
                            with path.open("rb") as source_file:
                                target_file.write(source_file.read())

    return tempdir


def wine_to_real_path(path: str) -> str:
    """
    Turn a Wine file path (C:\\xyz) into a local filesystem path (~/.wine/xyz)
    """
    return subprocess.check_output(('winepath', path)).strip().decode()


def unescape_winereg(value: str):
    """Remove quotes and escapes from a Wine registry value"""
    return value.strip('"').replace(r'\\\\', '\\')


def wine_srcdir_proposals() -> Generator[str, None, None]:
    """Yield a list of directory names where an installation might be found"""
    if "WINEPREFIX" in os.environ:
        yield "$WINEPREFIX/" + STANDARD_PATH_IN_32BIT_WINEPREFIX
        yield "$WINEPREFIX/" + STANDARD_PATH_IN_64BIT_WINEPREFIX
        yield "$WINEPREFIX/" + STANDARD_PATH_IN_WINEPREFIX_STEAM
    yield "~/.wine/" + STANDARD_PATH_IN_32BIT_WINEPREFIX
    yield "~/.wine/" + STANDARD_PATH_IN_64BIT_WINEPREFIX
    yield "~/.wine/" + STANDARD_PATH_IN_WINEPREFIX_STEAM

    # CrossOver bottles on macOS
    if platform.system() == "Darwin":
        crossover_root = Path.home() / "Library/Application Support/CrossOver/Bottles"
        if crossover_root.is_dir():
            for bottle in crossover_root.iterdir():
                if bottle.is_dir():
                    yield str(bottle / STANDARD_PATH_IN_32BIT_WINEPREFIX)
                    yield str(bottle / STANDARD_PATH_IN_64BIT_WINEPREFIX)
                    yield str(bottle / STANDARD_PATH_IN_WINEPREFIX_STEAM)

    try:
        info("using the wine registry to query an installation location...")
        # get wine registry key of the age installation
        with tempfile.NamedTemporaryFile(mode='rb') as reg_file:
            if not subprocess.call(('wine', 'regedit', '/E', reg_file.name,
                                    REGISTRY_KEY)):

                reg_raw_data = reg_file.read()
                try:
                    reg_data = reg_raw_data.decode('utf-16')
                except UnicodeDecodeError:
                    # this is hopefully enough.
                    # if it isn't, feel free to fight more encoding problems.
                    reg_data = reg_raw_data.decode('utf-8', errors='replace')

                # strip the REGEDIT4 header, so it becomes a valid INI
                lines = reg_data.splitlines()
                del lines[0:2]

                reg_parser = ConfigParser()
                reg_parser.read_string(''.join(lines))
                for suffix in REGISTRY_SUFFIX_AOK, REGISTRY_SUFFIX_TC:
                    reg_key = REGISTRY_KEY + suffix
                    if reg_key in reg_parser:
                        if '"InstallationDirectory"' in reg_parser[reg_key]:
                            yield wine_to_real_path(unescape_winereg(
                                reg_parser[reg_key]['"InstallationDirectory"']))
                        if '"EXE Path"' in reg_parser[reg_key]:
                            yield wine_to_real_path(unescape_winereg(
                                reg_parser[reg_key]['"EXE Path"']))

    except OSError as error:
        dbg("wine registry extraction failed: %s", error)


def test_parse_steam_library_paths():
    """Unit tests for libraryfolders.vdf path extraction."""
    modern = '''
"libraryfolders"
{
	"0"
	{
		"path"		"/Users/demo/Library/Application Support/Steam"
	}
	"1"
	{
		"path"		"/Volumes/Games/SteamLibrary"
	}
}
'''
    paths = parse_steam_library_paths(modern)
    assert paths == [
        "/Users/demo/Library/Application Support/Steam",
        "/Volumes/Games/SteamLibrary",
    ], paths

    legacy = '''
"LibraryFolders"
{
	"TimeNextStatsReport"		"123"
	"ContentStatsID"		"456"
	"1"		"/home/user/.local/share/Steam"
	"2"		"/mnt/ssd/SteamLibrary"
}
'''
    paths = parse_steam_library_paths(legacy)
    assert "/home/user/.local/share/Steam" in paths
    assert "/mnt/ssd/SteamLibrary" in paths
    assert "123" not in paths


def test_resolve_source_dir_override():
    """Unit tests for CLI/env source-dir override resolution."""
    import tempfile as tmpmod
    import unittest.mock as mock

    with tmpmod.TemporaryDirectory() as tmp:
        # CLI wins over env
        with mock.patch.dict(os.environ, {"OPENAGE_SOURCE_DIR": "/nonexistent"}, clear=False):
            assert resolve_source_dir_override(tmp) == expand_relative_path(tmp)

        # Env OPENAGE_SOURCE_DIR used when CLI unset
        with mock.patch.dict(os.environ, {"OPENAGE_SOURCE_DIR": tmp}, clear=False):
            os.environ.pop("AGE2DIR", None)
            assert resolve_source_dir_override(None) == expand_relative_path(tmp)

        # Invalid override is ignored
        with mock.patch.dict(os.environ, {}, clear=False):
            for key in SOURCE_DIR_ENV_VARS:
                os.environ.pop(key, None)
            assert resolve_source_dir_override("/no/such/openage/path") is None
            assert resolve_source_dir_override(None) is None
