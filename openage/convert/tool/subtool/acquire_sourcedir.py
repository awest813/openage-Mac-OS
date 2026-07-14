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

# Well-known Steam app folder names under steamapps/common/.
# Used when expanding libraryfolders.vdf library roots.
STEAM_COMMON_GAME_DIRS = (
    "AoEDE",
    "Age2HD",
    "AoE2DE",
    "STAR WARS - Galactic Battlegrounds Saga",
)


def expand_relative_path(path: str) -> AnyStr:
    """Expand relative path to an absolute one, including abbreviations like
    ~ and environment variables"""
    return os.path.realpath(os.path.expandvars(os.path.expanduser(path)))


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


def query_source_dir(proposals: set[str]) -> AnyStr:
    """
    Query interactively for a conversion source directory.
    Lists proposals and allows selection if some were found.
    """

    if proposals:
        print("\nPlease select an Age of Empires installation directory.")
        print("Insert the index of one of the proposals, or any path:")

        proposals = sorted(proposals)
        for index, proposal in enumerate(proposals):
            print(f"({index}) {proposal}")

    else:
        print("Could not find any installation directory "
              "automatically.")
        print("Please enter an AOE2 install path manually.")

    while True:
        user_selection = input("> ")
        if user_selection.isdecimal() and int(user_selection) < len(proposals):
            sourcedir = proposals[int(user_selection)]
        else:
            sourcedir = user_selection
        sourcedir = expand_relative_path(sourcedir)
        if Path(sourcedir).is_dir():
            break
        warn("No valid existing directory: %s", sourcedir)

    return sourcedir


def parse_steam_library_paths(vdf_text: str) -> list[str]:
    """
    Extract Steam library root paths from a libraryfolders.vdf body.

    Supports both the modern nested-block format and the older flat
    `"N" "/path"` style. Path values are returned as written (not expanded).
    """
    paths: list[str] = []
    seen: set[str] = set()

    def add(path: str) -> None:
        path = path.replace("\\\\", "\\")
        if path not in seen:
            seen.add(path)
            paths.append(path)

    # Modern format: "path"   "/some/library"
    for match in re.finditer(
        r'"path"\s*"([^"]+)"',
        vdf_text,
        flags=re.IGNORECASE,
    ):
        add(match.group(1))

    # Legacy format: numeric library id key → path value on its own line.
    # Require the *key* to be digits so values like ContentStatsID="456"
    # do not create false matches when scanned as `"456" "NextKey"`.
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
    """
    Return likely libraryfolders.vdf locations for the current platform.
    """
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
    """
    Resolve Steam library root directories from libraryfolders.vdf files.
    """
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
    """
    Propose existing steamapps/common/<game> directories across all Steam libraries.
    """
    proposals: set[str] = set()
    for root in steam_library_roots():
        common = root / "steamapps" / "common"
        if not common.is_dir():
            # Some VDF paths already point at the steam root that contains
            # steamapps/; others point directly at a library folder that
            # *is* the parent of steamapps/.
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
    """
    Collect steamapps/common/<folder> basenames declared in edition install paths.
    """
    names: set[str] = set(STEAM_COMMON_GAME_DIRS)
    for edition in avail_game_eds:
        for platform_paths in edition.install_paths.values():
            for path in platform_paths:
                normalized = path.replace("\\", "/")
                marker = "/steamapps/common/"
                if marker not in normalized.lower():
                    continue
                # Keep the original casing from the path after the marker.
                idx = normalized.lower().index(marker) + len(marker)
                folder = normalized[idx:].strip("/")
                if folder:
                    names.add(folder.split("/")[0])
    return names


def acquire_conversion_source_dir(
    avail_game_eds: list[GameEdition],
    prev_srcdir_paths: set[str] = None
) -> Path:
    """
    Acquires source dir for the asset conversion.

    Returns a file system-like object that holds all the required files.
    """
    try:
        # TODO: use some sort of GUI for this (GTK, QtQuick, zenity?)
        #       probably best if directly integrated into the main GUI.
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

        # Expand proposals across Steam library folders (libraryfolders.vdf).
        # Essential on macOS/Linux when games live on a secondary drive.
        folder_names = game_folder_names_from_editions(avail_game_eds)
        proposals.update(steam_game_dir_proposals(folder_names))

        # Wine / CrossOver prefixes (common on macOS for classic editions).
        for wine_path in wine_srcdir_proposals():
            expanded = expand_relative_path(wine_path)
            if Path(expanded).is_dir():
                proposals.add(expanded)

        use_trial = False
        if len(proposals) == 0:
            print("\nopenage requires a local game installation for conversion")
            print("but no local installation could be found automatically.")
            use_trial = wanna_download_trial()

        if use_trial:
            sourcedir = download_trial()

        else:
            sourcedir = query_source_dir(proposals)

    except KeyboardInterrupt:
        print("\nInterrupted, aborting")
        sys.exit(0)
    except EOFError:
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

    # CrossOver / Game Porting Toolkit-style prefixes on macOS
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
		"label"		""
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
    # Numeric non-path values must be ignored.
    assert "123" not in paths
    assert "456" not in paths
