# Troubleshooting

## macOS

### `incompatible architecture` / wrong Homebrew prefix

Apple Silicon Homebrew lives at `/opt/homebrew`; Intel at `/usr/local`.
Run `brew --prefix` and make sure `./configure --prefix` and
`--compiler="$(brew --prefix llvm)/bin/clang++"` use that tree. Mixing
prefixes produces link or dyld load failures.

### Gatekeeper rejects a downloaded release

Release archives are ad-hoc signed. After extracting:

```bash
xattr -dr com.apple.quarantine ./openage-*-macos-*
```

### Converter does not find my Steam library

Games on a secondary Steam library (external disk) are discovered via
`libraryfolders.vdf`. Confirm Steam lists the library under Settings →
Storage, then re-run conversion. Classic editions installed through Wine or
CrossOver are also proposed automatically.

### GUI never appears / window stays black

Qt and Cocoa must run on the main thread. This fork already does that on
`__APPLE__`. If you changed engine threading, restore presenter-on-main /
simulation-on-worker for macOS builds.

## Windows Installer

### More than one python installation

If you have two (or more) different python installations on your computer edit the `openage.bat` in the install directory:
Replace the line `python.exe -m openage` with `call "%INST_DIR%\python\python.exe" -m openage` to start `python.exe` explicitly.

## Asset Conversion

### Error: *No valid game version(s) could not be detected in <folder>*

Check if you have passed the **root folder** of the game to the converter and not a subfolder.

If that doesn't help, you could have a mod installed that messes with the detection algorithm.
In that case, you should reinstall a clean unmodded version of the game and retry the conversion.

### Conversion raises exception when converting *The Conquerors* 1.0c

Make sure you don't have *UserPatch*, compatibility patches or modifications installed that make
changes to the original asset files.

If you have Wololo Kingdoms and various mods installed that change the base assets the converter will not work.
A workaround would be to make a backup of your AGE2 directory and let the converter run on that backup. In that
backup at subfolder `AGE2/resources` delete all files ***except*** folders. Another workaround would be to
backup your AGE2 folder and redownload it to have a clean install. After conversion you can replace
it with the backup.
