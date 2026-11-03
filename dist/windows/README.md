# Jinra Windows installer

`jinra.iss` builds a per-user x64 installer with Inno Setup 6. It creates a portable
`user` directory beside the program, a Start menu entry, an optional desktop
shortcut and an uninstaller. Installation and upgrades replace only program and
license files. The user directory is preserved during uninstallation.

Pass a clean package folder containing `jinra.exe`, `LICENSE.txt`, `LICENSES` and
`GETTING_STARTED.txt`. Any required DLLs and `plugins` can also be included. Do not
distribute a development profile, keys, firmware, saves or shader caches.

The `AppVersion` preprocessor define must match the root `VERSION` file. The update
checker expects the release tag `v<VERSION>` and installer asset
`Jinra-Setup-<VERSION>-x64.exe`, uploaded as a GitHub Release asset so its SHA-256
digest is available from GitHub's release API.

Example:

```powershell
./dist/windows/build-installer.ps1 -PayloadDir ./clean-package -OutputDir ./build-installer
```

Use `-DTestMode=1` when invoking ISCC directly to disable shortcuts and the uninstall
registry entry for isolated install/upgrade/uninstall validation. Do not publish
that test-only installer. The first preview installer is unsigned.

This packaging and update integration was developed with AI assistance.
