<!--
SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
SPDX-FileCopyrightText: 2018 yuzu Emulator Project
SPDX-License-Identifier: GPL-3.0-or-later
-->

<p align="center"><img src="dist/jinra.png" alt="Jinra" width="160"></p>

# Jinra

Jinra is an experimental Nintendo Switch and Switch 2 emulator. This source
update brings the Fritchii v6 engine into Jinra, with the Jinra Home library,
controller navigation, and loading screen.

## Update notes

- Restored per-game settings for Zelda: Ocarina of Time, including the
  high-refresh controls.
- Updated the performance overlay to match Jinra's interface.
- Split shader preparation by compatible rendering configuration. A Windows
  test verified reuse when switching from 1× to 1.5× and back to 1×.
- Shader precompile system by Frizti (Fritchii). Jinra modifies the loading
  screen and the way preparation progress and results are stored.

## Build and credits

See the [build guide](docs/Build.md). The GUI target is **yuzu** and the
Windows executable is **jinra.exe**. Published installers and portable
builds are listed under [Jinra releases](https://github.com/JinWooPT/Jinra/releases).

Upstream source: [Fritchii v6](https://github.com/Frizti/Fritchii/releases/tag/fritchii-v6).
Dynamic FPS by Dark. See [LICENSE.txt](LICENSE.txt) and the
[additional terms for the precompile system](LICENSES/Fritchii-Additional-Terms.txt).
The [shader recipes](LICENSES/Fritchii-Recipe.txt) have separate terms.
