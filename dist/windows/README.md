# Jinra for Windows

The Windows packages use Clang, the Microsoft x64 toolchain and Windows SDK,
CMake and Ninja. Configure a Release build with Qt and SDL3 enabled, then
build the `yuzu` target to produce `jinra.exe`. See the
[build guide](../../docs/Build.md) for dependency setup.

Release builds use ThinLTO. Set `JINRA_BUILD_PLATFORM` to `windows-x86_64`
for Standard or `windows-x86_64-pgo` for PGO. For PGO, add `-fprofile-use`
with the absolute path to a compatible Windows profile to the compiler flags.
The Windows Jinra updater is built independently of `ENABLE_UPDATE_CHECKER`.

Use `build-installer.ps1` with Inno Setup 6. Stage `jinra.exe`,
`nx2_switches.txt`, `LICENSE.txt`, `LICENSES`, `GETTING_STARTED.txt` and
the app-local runtime libraries in the payload. The packages carry forward the 2.0.2 command-line executable and preserve any existing installed copy. Use `-Pgo`
to package the PGO build.

Publish the installers under the GitHub release tag matching `VERSION` with
a `v` prefix. For example, the `v2.0.3` release uses
`Jinra-Setup-2.0.3-x64.exe` for Standard and
`Jinra-Setup-2.0.3-x64-pgo.exe` for PGO. The updater checks the installer
name, size and SHA-256 digest from the release metadata.
