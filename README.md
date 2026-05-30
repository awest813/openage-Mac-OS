[![openage](/assets/logo/banner.svg)](http://openage.dev)

**openage-Mac-OS** — First-class macOS engine for RTS games
===========================================================

A macOS-focused fork of [openage](https://openage.dev), a free engine clone of the *Genie Engine* used by *Age of Empires*, *Age of Empires II (HD)*, and *Star Wars: Galactic Battlegrounds*.

[![macOS CI](https://github.com/awest813/openage-Mac-OS/actions/workflows/macosx-ci.yml/badge.svg)](https://github.com/awest813/openage-Mac-OS/actions/workflows/macosx-ci.yml)
[![GPL licensed](/assets/doc/license.svg)](/legal/GPLv3)

---

## 🍎 What is this fork?

This is a **specialized fork** targeting native macOS development. It delivers:

- **🔧 First-class macOS support** for Apple Silicon (`arm64`) and Intel (`x86_64`) architectures
- **📦 Pre-built releases** optimized for each architecture
- **📚 macOS-specific documentation** covering dependencies, build variants, and troubleshooting
- **🚀 Native build system integration** with Homebrew and Apple toolchains

**Status:** Gameplay is under active development. This is not a finished game yet, but you can build and run it on macOS.

---

## 🚀 Quick Start

### System Requirements

- **macOS 13.0** (Ventura) or later
- **Xcode 14+** — Command Line Tools (minimal) or full Xcode (complete development environment)
  - Required for macOS 13 SDK and C++20 support
  - Check [macOS support matrix](/doc/macos-support-matrix.md) for latest version requirements
- **Homebrew** (for dependency management)

### Build & Run (macOS)

```bash
# Clone the repository
git clone https://github.com/awest813/openage-Mac-OS
cd openage-Mac-OS

# Install dependencies (run these commands from the repo directory)
brew install cmake python3 libepoxy freetype fontconfig harfbuzz opus opusfile qt6 libogg libpng toml11 eigen llvm

# Set up a Python virtual environment (recommended)
python3 -m venv venv
source venv/bin/activate

pip3 install --upgrade cython numpy mako lz4 pillow pygments setuptools toml

# Build and test
./configure --compiler="$(brew --prefix llvm)/bin/clang++" --download-nyan
make
make test
make run
```

**Note:** 
- For detailed macOS build guidance, including cross-architecture builds and custom compiler options, see [doc/build_instructions/macos.md](/doc/build_instructions/macos.md).
- **Alternative approach:** If you prefer not to use a virtual environment, use `pip3 install --upgrade --break-system-packages ...` (as shown in the official build guide).
  - ⚠️ This may conflict with system Python packages, so venv is recommended if you have other Python-dependent tools.

### Run the Game

Once built:

```bash
cd bin && ./run main
```

The game requires original Age of Empires assets (not included). The converter will help you extract them from your game installation if needed.

---

## 📖 Documentation

### Getting Started (macOS-specific)

| Purpose | Link |
| ------- | ---- |
| **macOS Build Setup** | [Building on macOS](/doc/build_instructions/macos.md) — dependency installation, compiler selection, cross-architecture builds |
| **macOS Support Policy** | [Support Matrix](/doc/macos-support-matrix.md) — OS versions, architectures, compiler requirements |
| **Troubleshooting** | [Troubleshooting Guide](/doc/troubleshooting.md) — common issues and solutions |

### Development & Contribution

| Purpose | Link |
| ------- | ---- |
| **Contributing** | [Contributing Guide](/doc/contributing.md) |
| **General Build Info** | [Building (all platforms)](/doc/building.md) |
| **Testing** | [Testing Guide](/doc/code/testing.md) |
| **Project Structure** | [Architecture Overview](/doc/project_structure.md) |
| **Development Practices** | [Development Guide](/doc/development.md) |

### For Game Players

| Purpose | Link |
| ------- | ---- |
| **Running the Game** | [Running Guide](/doc/running.md) |
| **Asset Conversion** | [Asset Conversion](/doc/media_convert.md) |
| **Gameplay Info** | [Reverse Engineering Docs](/doc/reverse_engineering) |

---

## 💬 Get Help & Community

- **macOS-specific issues?** → [Fork Issues](https://github.com/awest813/openage-Mac-OS/issues)
- **Questions or discussions?** → [Fork Discussions](https://github.com/awest813/openage-Mac-OS/discussions)
- **Real-time chat** → [Matrix: #sfttech:matrix.org](https://matrix.to/#/#sfttech:matrix.org)
- **Upstream project** → [openage.dev](https://openage.dev)
- **Blog & News** → [blog.openage.dev](https://blog.openage.dev)

---

## 📋 Project Status

- **Latest Fork Release:** [Releases](https://github.com/awest813/openage-Mac-OS/releases)
- **Architecture Support:**
  - ✅ **Apple Silicon (arm64)** — Primary support
  - ✅ **Intel (x86_64)** — Primary support
  - 🔄 **Universal 2 binaries** (single binary for both arm64 + x86_64) — Planned for future release

---

## 📜 License

GNU GPLv3 or later. See [copying.md](/copying.md) and [legal/GPLv3](/legal/GPLv3) for details.

The project uses original game assets for gameplay but does not ship them.
