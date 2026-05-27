# Instructions for macOS users

This fork targets **first-class macOS support** on both Apple Silicon (`arm64`)
and Intel (`x86_64`).  See [doc/macos-support-matrix.md](/doc/macos-support-matrix.md)
for the full compatibility policy.

## Prerequisite steps

- XCode ≥ 14 (required for macOS 13 SDK and C++20 support)
- Install [Homebrew](http://brew.sh). If you use some other package manager, you're on your own :)

```bash
brew update-reset && brew update
brew install --cask font-dejavu
brew install cmake python3 libepoxy freetype fontconfig harfbuzz opus opusfile qt6 libogg libpng toml11 eigen
brew install llvm
pip3 install --upgrade --break-system-packages cython numpy mako lz4 pillow pygments setuptools toml
```

You will also need [nyan](https://github.com/SFTtech/nyan/blob/master/doc/building.md) and its dependencies:

```bash
brew install flex make
```

Optionally, for documentation generation:

```bash
brew install doxygen
```

## Clone the repository

```bash
git clone https://github.com/awest813/openage-Mac-OS
cd openage-Mac-OS
```

## Homebrew prefix by architecture

Homebrew installs to different prefixes depending on the host architecture:

| Architecture    | Homebrew prefix   |
| --------------- | ----------------- |
| Apple Silicon   | `/opt/homebrew`   |
| Intel           | `/usr/local`      |

The `brew --prefix` command always returns the correct prefix for the
currently running shell, so the configure commands below work on both without
changes.

## Building

We recommend using Homebrew's LLVM rather than Apple Clang, as Apple Clang is
often out of date and can cause compilation errors.

### Native build (recommended — builds for your machine's architecture)

```bash
./configure --compiler="$(brew --prefix llvm)/bin/clang++" --download-nyan
```

### Explicitly targeting Apple Silicon (arm64)

```bash
./configure \
  --compiler="$(brew --prefix llvm)/bin/clang++" \
  --target-arch=arm64 \
  --download-nyan
```

### Explicitly targeting Intel (x86_64)

```bash
./configure \
  --compiler="$(brew --prefix llvm)/bin/clang++" \
  --target-arch=x86_64 \
  --download-nyan
```

### Universal2 fat binary (arm64 + x86_64)

Building a universal binary requires every dependency to be a universal2
library.  This mode is **experimental** and intended for advanced users.

```bash
./configure \
  --compiler="$(brew --prefix llvm)/bin/clang++" \
  --target-arch=universal2 \
  --download-nyan
```

### Minimum macOS deployment target

The default deployment target is `13.0` (macOS Ventura).  Override with:

```bash
./configure \
  --compiler="$(brew --prefix llvm)/bin/clang++" \
  --macos-deployment-target=14.0 \
  --download-nyan
```

### Trigger the build

```bash
make -j$(sysctl -n hw.ncpu)
```

## Testing

```bash
make test
```

## Running

```bash
make run
```
or
```bash
cd bin && ./run
```

Try `./run --help` for available options.

## Creating documentation

```bash
make doc
```
For more options and details, refer to [doc/README.md][/doc/README.md]

## Troubleshooting

### Architecture mismatch

If you see errors like `incompatible architecture`, your Homebrew dependencies
were built for a different architecture than the one you are targeting.

- On Apple Silicon: make sure you are using the Homebrew installed at
  `/opt/homebrew`.  Running `brew --prefix` should print `/opt/homebrew`.
- On Intel: `brew --prefix` should print `/usr/local`.
- If you need x86_64 on an Apple Silicon machine, open a dedicated shell with
  `arch -x86_64 /bin/zsh` before running Homebrew or `./configure`.

### Rosetta 2

Running an Intel (x86_64) binary under Rosetta 2 on Apple Silicon is **not
an officially supported configuration**.  Use a native arm64 build instead.
See [doc/macos-support-matrix.md](/doc/macos-support-matrix.md) for the
Rosetta policy.

### LLVM path

If `brew --prefix llvm` returns an unexpected path, you can find the exact
path with:

```bash
brew info llvm | grep Cellar
```

Pass the full path to `clang++` manually:

```bash
# Apple Silicon example
./configure --compiler=/opt/homebrew/opt/llvm/bin/clang++ --download-nyan
# Intel example
./configure --compiler=/usr/local/opt/llvm/bin/clang++ --download-nyan
```

