# macOS Support Matrix

This document defines the official compatibility policy for the
`openage-Mac-OS` fork, which targets first-class macOS support on both
**Apple Silicon** and **Intel** hardware.

---

## Supported Architectures

| Architecture | Chip family        | Homebrew prefix   | Supported? |
| ------------ | ------------------ | ----------------- | ---------- |
| `arm64`      | Apple Silicon (M1+)| `/opt/homebrew`   | ✅ Primary  |
| `x86_64`     | Intel              | `/usr/local`      | ✅ Primary  |
| `universal2` | arm64 + x86_64 fat | both              | 🔄 Optional (future) |

Both `arm64` and `x86_64` are **tier-1** targets.  Every CI run and every
release must pass on both architectures.  `universal2` fat binaries are
planned for a future release once per-arch parity is stable.

---

## Minimum macOS Version

| Requirement            | Value       | Rationale                                       |
| ---------------------- | ----------- | ----------------------------------------------- |
| Minimum OS version     | **13.0** (Ventura) | Supports both arm64 and x86_64; ships Qt 6.2+; broad Homebrew support |
| Xcode requirement      | **≥ 14**    | Required for macOS 13 SDK and C++20 support     |
| macOS SDK target       | `13.0`      | Set via `MACOSX_DEPLOYMENT_TARGET=13.0`         |

---

## Compiler Toolchain Policy

| Option                    | Status     | Notes                                                    |
| ------------------------- | ---------- | -------------------------------------------------------- |
| Homebrew LLVM (clang++)   | ✅ Recommended | Predictable version, C++20 complete                  |
| Apple Clang (Xcode)       | ⚠️ Conditional | Acceptable only if Xcode ≥ 14; check C++20 coverage |
| GCC (Homebrew)            | 🔴 Not supported | macOS linking quirks; not tested                    |

Use Homebrew LLVM for the most reliable builds:

```bash
# Apple Silicon
brew install llvm
./configure --compiler="$(brew --prefix llvm)/bin/clang++"

# Intel
brew install llvm
./configure --compiler="$(brew --prefix llvm)/bin/clang++"
```

---

## Architecture Selection at Build Time

Pass `--target-arch` to `./configure`:

| Flag                          | Effect                                                   |
| ----------------------------- | -------------------------------------------------------- |
| `--target-arch native`        | Build for the host machine's native arch (default)       |
| `--target-arch arm64`         | Force arm64 target (e.g. cross-build on Intel)           |
| `--target-arch x86_64`        | Force x86_64 target (e.g. cross-build on Apple Silicon)  |
| `--target-arch universal2`    | Produce a fat binary for both architectures              |

Equivalent direct CMake flags:

```bash
cmake -DCMAKE_OSX_ARCHITECTURES=arm64 -DMACOSX_DEPLOYMENT_TARGET=13.0 ...
cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 -DMACOSX_DEPLOYMENT_TARGET=13.0 ...
cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DMACOSX_DEPLOYMENT_TARGET=13.0 ...
```

---

## Rosetta 2 Policy

Running the `x86_64` binary under Rosetta 2 on Apple Silicon hardware is
**not an officially supported configuration** in this fork.  Use a native
`arm64` build instead.

If Rosetta is unavoidable (e.g. a dependency is `x86_64`-only), the
following caveats apply:

- Homebrew must be installed under `/usr/local` (the Intel prefix).
- Pass `--target-arch x86_64` to `./configure`.
- Open a dedicated Terminal shell via `arch -x86_64 /bin/zsh` before
  configuring and building.

---

## Dependency ABI Policy

Each architecture has its own Homebrew installation:

| Architecture | Homebrew prefix   | ABI           |
| ------------ | ----------------- | ------------- |
| `arm64`      | `/opt/homebrew`   | arm64-apple-macos13 |
| `x86_64`     | `/usr/local`      | x86_64-apple-macos13 |

**Never mix libraries from both prefixes** in a single build.  The build
system automatically resolves the correct prefix via `brew --prefix`, which
returns the arch-appropriate path.

For `universal2` builds: each dependency must itself be a universal2
library.  Build each dependency with
`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` or install via MacPorts
universal packages.

---

## Release Artifact Policy

| Artifact type       | Architectures      | Signing                          |
| ------------------- | ------------------ | -------------------------------- |
| Development builds  | per-arch           | unsigned                         |
| Release DMG / tarball | per-arch (arm64, x86_64) | ad-hoc signed (unsigned OK for source builds) |
| Public release DMG  | arm64 + x86_64     | Developer ID signed (if Apple developer certificate available) |

Artifacts are named using the scheme:

```
openage-<version>-macos-<arch>.<ext>
```

Examples:
- `openage-0.6.1-macos-arm64.tar.gz`
- `openage-0.6.1-macos-x86_64.tar.gz`

---

## CI Runner Matrix

| Runner label   | Architecture | macOS version |
| -------------- | ------------ | ------------- |
| `macos-15`     | arm64        | 15.x          |
| `macos-13`     | x86_64       | 13.x          |

CI jobs are pinned to these specific runner labels to guarantee architecture
coverage.  `macos-latest` is **not used** in CI to prevent silent drift.

---

## Known Limitations

- `inotify` is Linux-only and disabled on macOS (expected).
- `libbacktrace` from GCC is not available on macOS; backtraces fall back to
  platform stack unwinding.
- Memory and thread sanitizers (`--sanitize=mem` / `--sanitize=thread`) are
  disabled on Apple platforms due to missing kernel support.

---

## Version History

| Date       | Change                                                    |
| ---------- | --------------------------------------------------------- |
| 2026-05-27 | Phase H: native macOS Steam paths, default_dirs tests, release ccache |
| 2026-05-27 | Initial support matrix for Apple Silicon + Intel fork     |
