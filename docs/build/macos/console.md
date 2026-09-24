# macOS - Terminal

Native Apple Silicon (arm64) build from the command line. Produces a full,
playable client as an app bundle: the engine, the `osx-arm64` network library
(`MUnique.Client.Library.dylib`), and SDL GPU rendering through Metal. This is
the same configuration CI validates (`macos-arm64`, Release, editor OFF),
plus the optional editor build.

See [the build guide](../README.md) for shared concepts (editor flag, the
network library, output layout).

## Prerequisites (one-time)

Xcode Command Line Tools (`xcode-select --install`) and
[Homebrew](https://brew.sh) for Apple Silicon (`/opt/homebrew`).

```bash
# Build tools, native dependencies, optional shader tools
brew install cmake ninja pkgconf openssl@3 curl brotli jpeg-turbo
brew install glslang spirv-cross   # only for -DMU_ENABLE_SHADER_COMPILATION=ON
```

The .NET 10 SDK builds the network library with Native AOT. Install it without
`sudo` into your home directory:

```bash
curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0 --install-dir "$HOME/.dotnet"
```

Then add this to `~/.zshrc` (and to the environment of any IDE you launch from
the Dock, which does not read `~/.zshrc`):

```bash
export DOTNET_ROOT="$HOME/.dotnet"
export PATH="$DOTNET_ROOT:$DOTNET_ROOT/tools:$PATH"
```

`brew install --cask dotnet-sdk` works too, but its installer asks for an
administrator password.

## Configure and build

Player build, identical to CI:

```bash
cmake --preset macos-arm64 -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build --preset macos-arm64-release
```

### Editor build

The editor (`ENABLE_EDITOR=ON`: ImGui toolbar, Item/Skill/Dev editors and the
Map Editor) has its own preset and build directory,
`out/build/macos-arm64-mueditor`:

```bash
cmake --preset macos-arm64-mueditor -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build --preset macos-arm64-mueditor-release --target Main
```

Run it from `out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS`
with `./Main --editor`. The Map Editor's usage, including macOS keys and where
saves land, is in
[`src/MuEditor/UI/MapEditor/MAP_EDITOR.md`](../../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md).

The generator is Ninja Multi-Config, so `--config Debug` builds a debug client
into the same build directory without reconfiguring. Debug builds enable SDL GPU
validation and are noticeably slower. Metal's own API validation, which stops
the client at the first invalid GPU call, is separate: a Debug build alone does
not switch it on. Start a client of either configuration with
`MTL_DEBUG_LAYER=1`, for example `MTL_DEBUG_LAYER=1 ./Main --editor`.

`OPENSSL_ROOT_DIR` is required: CMake does not find Homebrew's keg-only OpenSSL
on its own. Homebrew's `curl` is found through the macOS SDK instead.

## Run

The build writes an app bundle and copies the assets, `config.ini`, the shaders
and the network library into it:

```
out/build/macos-arm64/src/Release/Main.app/Contents/MacOS/
  Main
  MUnique.Client.Library.dylib
  config.ini
  shaders/
  Data/  fonts/
```

Run the executable from that directory so it finds its files:

```bash
cd out/build/macos-arm64/src/Release/Main.app/Contents/MacOS
./Main
```

The client writes its log to `MuError.log` in that directory. The repository's
`Data/` contains no `Sound/` or `Music/` files, so expect audio load errors
there and a silent client until you add them. A missing music track is reported
once per scene that requests it and then skipped; the client keeps running.

Set the server in `config.ini` (`[CONNECTION SETTINGS]` `ServerIP` /
`ServerPort`). An [OpenMU](https://github.com/MUnique/OpenMU) server listens for
this client on port **44406** (44405 is for the original client). `/u<ip>` and
`/p<port>` on the command line override the file, and `--editor` starts an
editor build with the editor open (F12 toggles it; fn+F12 when the top row
sends media keys).

## Asset tools

The same build produces `out/build/macos-arm64/tools/bmdconv/Release/bmdconv`, the model
converter described in [the asset pipeline](../../asset-pipeline.md). Python and Blender
scripts live in `tools/`.

### Item Editor concept renders

**Render missing references** in the Item Editor renders items with Blender and the
[Blender Source Tools](https://github.com/Artfunkel/BlenderSourceTools) add-on, which imports
the game's models. The tool uses the first Blender it finds:

1. `--blender` or `MU_BLENDER`;
2. `/Applications/Blender.app`;
3. the art agents' `astra-tools/Blender.app` next to the checkout (`../astra-tools`), with the
   add-on in `../astra-tools/blender-user/scripts`, which the tool passes to Blender as
   `BLENDER_USER_SCRIPTS` unless you set it.

Where `astra-tools` exists nothing needs setting up. Otherwise install Blender into
`/Applications` and install and enable Blender Source Tools in it (Edit > Preferences >
Add-ons > Install from Disk).

## Tests

```bash
ctest --test-dir out/build/macos-arm64 --build-config Release --output-on-failure
```

The `macos-arm64` preset already sets `BUILD_TESTING=ON`; the tests run
natively.

## Troubleshooting

### `fatal error: 'cassert' file not found`

Apple clang searches `/Library/Developer/CommandLineTools/usr/include/c++/v1`
before the SDK. After a Command Line Tools update that directory can survive as
a partial leftover, and clang then never falls back to the complete libc++
headers in the SDK. Check it:

```bash
ls /Library/Developer/CommandLineTools/usr/include/c++/v1
```

If it exists but has no `cassert`, move it out of the way (the SDK copy is the
one Apple actually ships headers in):

```bash
sudo mv /Library/Developer/CommandLineTools/usr/include/c++/v1 ~/CLT-stale-cxx-v1-backup
```

Without `sudo`, point the build at the SDK headers instead:

```bash
cmake --preset macos-arm64 -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  "-DCMAKE_CXX_FLAGS=-nostdinc++ -isystem $(xcrun --sdk macosx --show-sdk-path)/usr/include/c++/v1"
```

Installing Homebrew LLVM (`brew install llvm`) also avoids it; the macOS
toolchain file prefers `/opt/homebrew/opt/llvm` when it exists.

### Wrong-architecture libraries on a Mac with two Homebrew installs

An older Intel Homebrew in `/usr/local` leaves x86_64 libraries (OpenSSL,
Brotli, jpeg-turbo) and an x86_64-only `pkg-config` behind. Symptoms are link
errors about files "built for macOS-x86_64", or the .NET Native AOT link of the
network library failing on Brotli. Make sure `/opt/homebrew/bin` precedes
`/usr/local/bin` on `PATH`, install `pkgconf` from the arm64 Homebrew, and
verify with `file -L` which library CMake picked (`CMakeCache.txt`).

### `ImportError: ... pyexpat ... Symbol not found: _XML_SetAllocTrackerActivationThreshold`

The `gen_wire_sizes.py` build step fails when an outdated Homebrew Python's
`pyexpat` does not match the system `libexpat`. Upgrade it
(`brew upgrade python@3.14`) or configure with the system Python:

```bash
cmake --preset macos-arm64 -DPython3_EXECUTABLE=/usr/bin/python3
```

### Network library not built

If configure prints `.NET SDK not found`, `dotnet` is not on the `PATH` of the
shell or IDE running CMake. The client still builds but cannot connect.
