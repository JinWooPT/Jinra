# Jinra for Windows

The Windows packages use Clang 22.1.8, the Microsoft x64 toolchain and Windows SDK,
CMake and Ninja. Configure a Release build with Qt and SDL3 enabled, then build
the yuzu and yuzu-cmd targets. Their outputs are jinra.exe and jinra-cli.exe.
The original build guide covers toolchain and dependency setup.

The release builds use ThinLTO. Set JINRA_BUILD_TAG to build-116 and set
JINRA_BUILD_PLATFORM to windows-x86_64 for Standard or windows-x86_64-pgo for PGO.
For PGO, add -fprofile-use with the absolute path to jinra-windows.profdata to
the compiler flags. The profile is in the source tree. Disable the inherited
update checker with ENABLE_UPDATE_CHECKER=OFF. The Jinra update checker is
included independently.

Package the outputs using build-installer.ps1 and Inno Setup 6. Supply a clean
payload with both executables, nx2_switches.txt, LICENSE.txt, LICENSES,
GETTING_STARTED.txt, and the required app-local runtime libraries. Copy the
contents of dist/recipes into the payload's recipes directory when present.
Use -Pgo for the PGO installer. The release tag must match VERSION and each
installer must have its corresponding edition suffix for the updater to
recognize it.
