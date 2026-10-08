# InterpFix

MetaHookSV plugin fixing packet entities disappearing when a high snapshot rate
overwrites the position history required by `ex_interp`. It keeps the oldest
valid pose when the requested time is outside the history window.

## Installation

Copy `InterpFix.dll` to `<mod>/metahook/plugins/` and the complete `interpfix`
catalog directory to `<mod>/metahook/gamedata/interpfix/`. Add `InterpFix.dll` to
`<mod>/metahook/configs/plugins.lst`. MetaHook API 109 or newer is required.

Both GoldSrc and SvEngine are supported through the shipped catalog: Half-Life
builds 3248, 3266, 3329, 3647, 4554, 6153, 8684, 10210; Sven Co-op builds 8948,
10257; and Cry of Fear build 5936. CS/CZ use their shared GoldSrc engine identity.
Unknown binaries or missing required symbols fail with a diagnostic.

The fix searches the complete 64-slot history and hooks `CL_InterpolateModel`
alongside `CL_FindInterpolationUpdates`: the original model function rejects a
negative time delta before checking whether both samples have equal timestamps.
The wrapper preserves original results and repairs only failure after a valid
history clamp. It does not change `ex_interp`, snapshot scheduling, or history
capacity. At very high snapshot rates the fallback pose is newer than the
requested interpolation time.

## Build and test

Windows, Visual Studio 2022 with MSVC x86, CMake 3.21+, and Python 3.8+ are required.

```powershell
cmake -S . -B build -A Win32 -DINTERPFIX_BUILD_TESTS=ON -DCMAKE_INSTALL_PREFIX=install
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release
```

Repeat with `Debug` for the other configuration. `METAHOOK_SOURCE_PATH` selects a
local, read-only SDK tree; otherwise the SDK is fetched from its `main` branch.
VC-LTL 5.3.1 is downloaded and SHA-256 verified, or provided through `VC_LTL_Root`.
`INTERPFIX_SYNC_GAMEDATA` defaults ON and validates all 11 catalog snapshots before
building. With it OFF, a previously generated catalog must be supplied for installation.
Build artifacts and synchronized catalogs stay outside Git.

Standalone Visual Studio F5 deployment is optional:

```powershell
cmake -S . -B build/launch -A Win32 -DMETAHOOKSV_ENABLE_LAUNCH_GAME=ON
```

The shared LaunchGame module supports `METAHOOKSV_GAME_DIRECTORY`,
`METAHOOKSV_GAME_APPID`, and `METAHOOKSV_GAME_MOD`; deployment requires an existing
MetaHook installation. See the [aggregator documentation](https://github.com/MetaHookSv/MetaHookSv#debugging).

Regression tests compile the production lookup, wrapper, and hook installation
code against the SDK. They cover history boundaries, angle wrapping, resets,
nested calls, 600/650/2000/2500 updates per second, and installation rollback.
They model the engine callback and do not replace in-game verification.

## References

- [MetaHookSv issue #887](https://github.com/MetaHookSv/MetaHookSv/issues/887)
- [Root cause analysis](https://github.com/MetaHookSv/MetaHookSv/issues/883#issuecomment-5854321213)
- [Reference engine fix](https://github.com/HLND2T/HLND2T-DiligentGraphics/commit/903fe7157d5f4afa027b2d8b4f59070fc673e311)

The history selection behavior follows the reference fix. SDK components retain
their own licensing terms.

## C/C++ formatting

Formatting uses [MetaHookSv/FormatValidation](https://github.com/MetaHookSv/FormatValidation)
and clang-format **23.1.3**, with the DiligentCore style (4 spaces, preserved include
order). Install the formatter for the Python interpreter used by CMake:

```sh
python -m pip install clang-format==23.1.3
cmake -S . -B build/format "-DFORMAT_VALIDATION_ONLY=ON"
cmake --build build/format --target format-check
cmake --build build/format --target format
```

The format-only configuration needs CMake 3.21+, Git, Python 3.9+ (CI uses 3.12),
and a build generator; `-G Ninja` works without Visual Studio. It prepares no native
SDK or game dependencies. Formatting targets are explicit and are not part of a
normal DLL build. With a Visual Studio generator, add `--config Debug` or
`--config Release` when building a formatting target.

The aggregate provides `FORMAT_VALIDATION_SOURCE_PATH=thirdparty/FormatValidation`.
Standalone components accept that CMake variable or its environment counterpart;
if empty, FetchContent downloads the fixed tooling commit. Quote relative paths,
for example `"-DFORMAT_VALIDATION_SOURCE_PATH=../../thirdparty/FormatValidation"`.
Configuration generates the ignored root `.clang-format` for editors; change the
shared style rather than that generated copy. An optional
`FORMAT_VALIDATION_CLANG_FORMAT_EXECUTABLE` selects an explicit formatter, whose
version must still match the pin.

Checks cover owned C/C++ files in `src/`, `include/`, and `tests/`, including
non-ignored new files. Repository-relative exclusions live in `.clang-format-ignore`.
Third-party sources and build artifacts are excluded. The `clang-format` workflow
checks the full scope on pushes, pull requests, and manual runs.
