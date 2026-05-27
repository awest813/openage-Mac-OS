# Copyright 2017-2024 the openage authors. See copying.md for legal info.

"""

Code for locating the game assets.

All access to game assets should happen through objects obtained from get().
"""

import os
import pathlib
import sys

# TODO: use os.pathsep for multipath variables

# Linux-specific dirs according to the freedesktop basedir standard:
# https://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
#
# concretely:
# $XDG_CONFIG_HOME = $HOME/.config
# $XDG_DATA_HOME = $HOME/.local/share
# $XDG_DATA_DIRS = /usr/local/share/:/usr/share/
# $XDG_CONFIG_DIRS = /etc/xdg
# $XDG_CACHE_HOME = $HOME/.cache
# $XDG_RUNTIME_DIR = /run/user/$UID
#
LINUX_DIRS = {
    "config_home": ("XDG_CONFIG_HOME", ("{HOME}/.config", {"HOME"})),
    "data_home": ("XDG_DATA_HOME", ("{HOME}/.local/share", {"HOME"})),
    "data_dirs": ("XDG_DATA_DIRS", ("/usr/local/share/:/usr/share/", {})),
    "config_dirs": ("XDG_CONFIG_DIRS", ("/etc/xdg", {})),
    "cache_home": ("XDG_CACHE_HOME", ("{HOME}/.cache", {"HOME"})),
    "runtime_dir": ("XDG_RUNTIME_DIR", ("/run/user/$UID")),
}


# macOS-specific dirs following Apple's file-system layout conventions.
# Preference is given to XDG env vars when set, so power users can override.
# Default paths:
#   ~/Library/Application Support  - config and data (per Apple HIG)
#   /Library/Application Support   - system-wide read-only data / config
#   ~/Library/Caches               - cache (purge-safe)
#   ~/Library/Caches               - runtime state (closest macOS equivalent)
MACOS_DIRS = {
    "config_home": ("XDG_CONFIG_HOME",
                    ("{HOME}/Library/Application Support", {"HOME"})),
    "data_home": ("XDG_DATA_HOME",
                  ("{HOME}/Library/Application Support", {"HOME"})),
    "data_dirs": ("XDG_DATA_DIRS",
                  ("/Library/Application Support", {})),
    "config_dirs": ("XDG_CONFIG_DIRS",
                    ("/Library/Application Support", {})),
    "cache_home": ("XDG_CACHE_HOME",
                   ("{HOME}/Library/Caches", {"HOME"})),
    "runtime_dir": ("XDG_RUNTIME_DIR",
                    ("{HOME}/Library/Caches", {"HOME"})),
}


# Windows-specific paths
WINDOWS_DIRS = {
    "config_home": ("APPDATA", (False, None)),
    "data_home": ("APPDATA", (False, None)),
    "config_dirs": ("ALLUSERSPROFILE", (False, None)),
    # TODO: other windows paths
}


def get_dir(which):
    """
    Returns directories used for data and config storage.
    returns pathlib.Path
    """

    platform_table = None

    if sys.platform.startswith("linux"):
        platform_table = LINUX_DIRS

    elif sys.platform.startswith("darwin"):
        platform_table = MACOS_DIRS

    elif sys.platform.startswith("win32"):
        platform_table = WINDOWS_DIRS

    else:
        raise RuntimeError(f"unsupported platform: '{sys.platform}'")

    if which not in platform_table:
        raise ValueError(f"unknown directory requested: '{which}'")

    # fetch the directory template
    env_var, (default_template, required_envs) = platform_table[which]

    # then create the result from the environment
    env_val = os.environ.get(env_var)

    if env_val:
        path = env_val

    elif default_template:
        env_vars = {var: os.environ.get(var) for var in required_envs}
        if not all(env_vars.values()):
            env_var_str = ', '.join([var for (var, val) in env_vars.items()
                                     if val is None])
            raise RuntimeError(f"could not reconstruct {which}, "
                               f"missing env variables: '{env_var_str}'")

        path = default_template.format(**env_vars)

    else:
        raise RuntimeError(f"could not find '{which}' in environment")

    return pathlib.Path(path)


def test():
    """
    Unit tests for get_dir() covering the MACOS_DIRS table.

    Verifies that:
    - default paths are derived from $HOME when no XDG override is set.
    - XDG env-var overrides take precedence on all platforms.
    - Requesting an unknown key raises ValueError.
    - Missing required env vars raise RuntimeError.
    """
    import unittest.mock as mock
    from .testing.testing import assert_raises

    fake_home = "/Users/testuser"
    xdg_vars = (
        "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_DATA_DIRS",
        "XDG_CONFIG_DIRS", "XDG_CACHE_HOME", "XDG_RUNTIME_DIR",
    )

    # --- macOS default-path tests (no XDG overrides set) ---
    macos_expectations = {
        "config_home": f"{fake_home}/Library/Application Support",
        "data_home":   f"{fake_home}/Library/Application Support",
        "data_dirs":   "/Library/Application Support",
        "config_dirs": "/Library/Application Support",
        "cache_home":  f"{fake_home}/Library/Caches",
        "runtime_dir": f"{fake_home}/Library/Caches",
    }

    # Patch platform and HOME; strip all XDG vars from the live environment
    saved = {k: os.environ.pop(k, None) for k in xdg_vars}
    saved_home = os.environ.get("HOME")
    try:
        os.environ["HOME"] = fake_home
        with mock.patch("sys.platform", "darwin"):
            for key, expected in macos_expectations.items():
                result = get_dir(key)
                assert result == pathlib.Path(expected), (
                    f"get_dir({key!r}) on darwin: expected {expected!r}, got {result!r}"
                )
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        if saved_home is None:
            os.environ.pop("HOME", None)
        else:
            os.environ["HOME"] = saved_home

    # --- XDG override takes precedence on macOS ---
    saved_cfg = os.environ.pop("XDG_CONFIG_HOME", None)
    saved_home2 = os.environ.get("HOME")
    try:
        os.environ["XDG_CONFIG_HOME"] = "/custom/config"
        os.environ["HOME"] = fake_home
        with mock.patch("sys.platform", "darwin"):
            result = get_dir("config_home")
            assert result == pathlib.Path("/custom/config"), (
                f"XDG_CONFIG_HOME override failed: got {result!r}"
            )
    finally:
        if saved_cfg is None:
            os.environ.pop("XDG_CONFIG_HOME", None)
        else:
            os.environ["XDG_CONFIG_HOME"] = saved_cfg
        if saved_home2 is None:
            os.environ.pop("HOME", None)
        else:
            os.environ["HOME"] = saved_home2

    # --- Unknown key raises ValueError ---
    saved_home3 = os.environ.get("HOME")
    try:
        os.environ["HOME"] = fake_home
        with mock.patch("sys.platform", "darwin"):
            with assert_raises(ValueError):
                get_dir("nonexistent_key")
    finally:
        if saved_home3 is None:
            os.environ.pop("HOME", None)
        else:
            os.environ["HOME"] = saved_home3

    # --- Missing HOME raises RuntimeError for HOME-dependent keys ---
    home_dependent_xdg = (
        "XDG_CONFIG_HOME", "XDG_DATA_HOME",
        "XDG_CACHE_HOME", "XDG_RUNTIME_DIR",
    )
    saved2 = {k: os.environ.pop(k, None) for k in home_dependent_xdg}
    saved_home4 = os.environ.pop("HOME", None)
    try:
        with mock.patch("sys.platform", "darwin"):
            with assert_raises(RuntimeError):
                get_dir("config_home")
    finally:
        for k, v in saved2.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        if saved_home4 is not None:
            os.environ["HOME"] = saved_home4
