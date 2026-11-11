<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

<p align="center"><img src="dist/jinra.png" alt="Jinra" width="160"></p>

# Jinra

Jinra is an experimental fork of [Eden-NS2](https://github.com/0x2593/Eden-NS2).
Jinra starts in Home, with a game library, favorites, recent games, and
dedicated screens for graphics, system, audio, and controls. Select
**Desktop mode** to use the desktop library and menu bar.

## Windows downloads

Download an installer or portable archive from
[GitHub Releases](https://github.com/JinWooPT/Jinra/releases). Both Standard and
PGO are Windows x64 builds. The PGO edition uses a gameplay profile to guide
compiler optimizations. Installed editions receive updates for their own edition.

For a portable install, extract the entire archive and run Jinra. It stores
saves and settings in the user directory beside the application. Installer
upgrades preserve that directory. Use your own game dumps and system files.

To check for an update from Home, select **Desktop mode**, then
**Help → Check for updates**.

## 1.0.0

Jinra can prepare Vulkan shaders for the matching Ocarina of Time revision
before gameplay. The loading screen reports progress and offers **Play now**
to continue shader preparation in the background. Prepared shaders remain
available across compatible updates. A change to the shader translator, driver
or relevant graphics settings can require preparation again.

Right-click a game and select **Sync Shaders** to look up a compatible shared
cache in the Jinra catalog. If the game already runs well with its locally
prepared cache, you can keep using it.

The Ocarina of Time startup and prepared cache reuse were verified on Windows
with a GeForce RTX 5090. The AMD Vulkan interpolation adjustment still needs
gameplay verification on affected Radeon hardware.

## Windows defaults and compatibility

Jinra includes the Vulkan safeguard for
pre-RDNA3 AMD Windows drivers. Those drivers can advertise cooperative matrices
through emulation. Jinra uses the shader fallback on affected GPUs to address
the Ocarina of Time crash during shader compilation. The same fix was verified
on an RX 6800 XT upstream. Jinra-specific gameplay verification on Radeon is pending.

Switch 2 titles select the 12 GiB memory layout at startup. Game compatibility,
rendering, and performance vary with the title, hardware, and driver.

On Windows, the first-run profile uses Vulkan at 1× in Docked mode with ScaleForce,
FXAA, 16× anisotropic filtering, Fast GPU accuracy, aggressive VRAM usage, GPU
ASTC decoding without recompression, and asynchronous shader compilation and
presentation. FSR 3 frame generation and supported in-game DLSS paths start
enabled. Existing configurations keep their saved settings.

New profiles use the Jinra name and logo. The initial Web username is jinra.
Existing profiles keep their names, preferences, and save associations.

## Interface

Home includes profile names and avatars, and a sound volume control under
**Settings → Audio**. Use **View → Show Performance Overlay** or **Ctrl+V**
to show FPS, frame time, process RAM, and GPU memory where the Vulkan driver
reports it.

The following screenshots show the Jinra interface from an earlier preview.

![Jinra library with Donkey Kong Bananza selected](docs/jinra/screenshots/library.jpg)

![Jinra controls with a DualSense controller](docs/jinra/screenshots/controls-dualsense.jpg)

## Build and license

See the [build guide](docs/Build.md) and [Windows packaging notes](dist/windows/README.md).
See [LICENSE.txt](LICENSE.txt) and [LICENSES](LICENSES) for applicable terms.
The Hello Kitty Island Adventure avatar artwork is credited to SANRIO and Sunblink.
