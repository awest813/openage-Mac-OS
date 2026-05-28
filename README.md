[![openage](/assets/logo/banner.svg)](http://openage.dev)
=========================================================

**openage-Mac-OS** is a macOS-focused fork of [openage](https://openage.dev), a free engine clone of the *Genie Engine* used by *Age of Empires*, *Age of Empires II (HD)*, and *Star Wars: Galactic Battlegrounds*.

The project uses original game assets for gameplay and conversion, but does not ship them.

[![macOS CI](https://github.com/awest813/openage-Mac-OS/actions/workflows/macosx-ci.yml/badge.svg)](https://github.com/awest813/openage-Mac-OS/actions/workflows/macosx-ci.yml)
[![GPL licensed](/assets/doc/license.svg)](/legal/GPLv3)

## What this fork focuses on

- First-class support for macOS on Apple Silicon (`arm64`) and Intel (`x86_64`)
- Release tarballs for both architectures
- Clear build and support guidance for macOS users

## Start here

| Topic | Link |
| ----- | ---- |
| macOS build guide | [doc/build_instructions/macos.md](/doc/build_instructions/macos.md) |
| macOS support policy | [doc/macos-support-matrix.md](/doc/macos-support-matrix.md) |
| General build docs | [doc/building.md](/doc/building.md) |
| Testing | [doc/code/testing.md](/doc/code/testing.md) |
| Troubleshooting | [doc/troubleshooting.md](/doc/troubleshooting.md) |
| Contributing | [doc/contributing.md](/doc/contributing.md) |

## Quick start

```bash
git clone https://github.com/awest813/openage-Mac-OS
cd openage-Mac-OS

# macOS users: follow doc/build_instructions/macos.md for dependency setup.
./configure --download-nyan
make
make test
make run
```

If you only want to run the already-built binary:

```bash
cd bin && ./run main
```

## Project status

Gameplay is still under active development, so this is not a finished game yet.
For the latest updates, see the [development blog](https://blog.openage.dev) and the [release page](https://github.com/awest813/openage-Mac-OS/releases).

## Get help

- [GitHub Issues](https://github.com/awest813/openage-Mac-OS/issues)
- [GitHub Discussions](https://github.com/awest813/openage-Mac-OS/discussions)
- [Matrix chat](https://matrix.to/#/#sfttech:matrix.org)

## License

GNU GPLv3 or later; see [copying.md](copying.md) and [legal/GPLv3](/legal/GPLv3).
