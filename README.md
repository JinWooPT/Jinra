<p align="center"><img src="dist/jinra.png" alt="Jinra" width="160"></p>

# Jinra

Jinra is an experimental fork of [Eden-NS2](https://github.com/0x2593/Eden-NS2),
building on the work of Eden and yuzu. It combines Switch 2 emulation changes,
Donkey Kong Bananza rendering and performance work, and a new desktop interface.

## Testing scope — please read first

**Donkey Kong Bananza is the only game tested with this version of Jinra.**

**The emulator has been optimized and tested on NVIDIA graphics cards under
Windows. Results on AMD graphics cards are unknown: AMD compatibility,
rendering quality, performance and stability have not been verified.**

Other games, Intel graphics, other GPU configurations and other operating systems
have not been validated by this project. The upstream project's compatibility
reports and screenshots are not Jinra test results.

| Game | Tested environment | Current scope |
| --- | --- | --- |
| Donkey Kong Bananza | Windows, NVIDIA GPU, Vulkan | Rendering fixes and performance improvements tested; optimization and stability work continue |
| All other games | Not tested with Jinra | Compatibility and performance unknown |

**Stable 60 FPS is a development target, not a guarantee.** Slowdowns and stutter
can still occur, particularly during destruction and in demanding areas. A brief
60 FPS reading does not establish sustained performance or crash-free gameplay.

## What is included

- Rendering, synchronization and buffer-handling work developed while testing
  Donkey Kong Bananza on NVIDIA hardware.
- Jinra branding, the samurai icon and a redesigned desktop library.
- Library search, Recent and Favorites views, and a selected-game panel.
- Xbox and DualSense layout previews, SDL device detection and physical button
  labels. Virtual-controller checks passed; physical-device behavior still needs
  broader verification.
- The existing interface-language catalog and advanced settings.
- Detection of `.nup` games in the library and file picker, with experimental
  NUP v1 loading for raw and Zstandard-compressed members, including
  a base program and its newest bundled update.

NUP support passed container-fixture tests. **Donkey Kong Bananza was also
successfully tested from a `.nup` dump by the project owner.** This does not
establish compatibility with other games or every NUP layout. Bundled DLC records
are recognized but are not mounted. See [NUP support](docs/nup-jinra.md) for details
and attribution.

## Defaults and performance

New configurations start with English and the Jinra theme, plus:

| Setting | Default |
| --- | --- |
| Graphics backend | Vulkan |
| Emulated memory | 12 GB |
| Console mode | Handheld |
| Resolution scale | 2x |
| Scaling filter | ScaleForce |
| Anti-aliasing | SMAA |
| GPU accuracy | High / Accurate |
| Asynchronous shader building | Enabled |
| Video decoding | GPU |
| VRAM usage mode | Aggressive |

These are the settings used during development, not universal recommendations
for every PC. Existing saved preferences and per-game overrides take precedence.
If 2x is too demanding for your system, try 1x. Resolution scaling multiplies the
game's internal render size, which can vary by game, scene and console mode.

See [the desktop interface notes](docs/jinra-ui.md) for controller, artwork,
language and profile behavior. New initial profiles use the Jinra name and logo;
existing profile identifiers and save associations are preserved.

## Running

Use your own game dumps and any required keys and firmware. No games, keys,
firmware, user profiles, saves or shader caches are distributed with Jinra.

Add your game folder in the Library or use **File > Load File**. Extracted dumps
are supported; for that layout, keep `romfs` alongside `exefs`.

The desktop executable is `jinra.exe` on Windows. Internal storage and some
configuration keys retain their upstream names for compatibility.

## Building from source

The existing CMake desktop target is still named `yuzu`; its output has been
renamed to `jinra`. General dependency instructions are in [docs/Deps.md](docs/Deps.md).

For Windows, the inherited [MSVC build helper](build-eden-msvc.bat) is available.
It requires Visual Studio or Build Tools with Desktop development with C++,
CMake, Ninja, Git for Windows and the dependencies described in the build docs.
The helper retains its upstream filename and some legacy output messages;
look for `jinra.exe` in the build output directory.

The local development build was validated with MSVC, Release, LTO and a locally
trained optimization profile. That private build directory and profile are not
part of the repository. Rebuilding with another toolchain or without that
profile may change performance; the published source is not a claim of identical
binaries across build environments.

Existing upstream Linux and other-platform build instructions remain in the
documentation. These platforms have not been tested with Jinra.

## Reporting results

Include the Jinra commit or version, game version, GPU model, driver version,
operating system and relevant settings. AMD reports are especially useful because
this project's AMD behavior is currently unknown. Do not attach game data, keys,
firmware or private user files.

## Credits, license and development disclosure

Jinra is derived from [0x2593/Eden-NS2](https://github.com/0x2593/Eden-NS2),
[Eden](https://git.eden-emu.dev/eden-emu/eden) and yuzu. Original copyright,
license and contributor notices are retained. The tested Jinra source was based
on Eden-NS2 commit `c3dd05fb279b0e5b330964bf9983e4a88edc326e`.

NUP loading is adapted from a55uka's format and Eglantine's GPL-licensed reader;
the pinned source and limitations are documented in [docs/nup-jinra.md](docs/nup-jinra.md).

This fork was developed with substantial AI assistance, including code changes,
tests, interface artwork and documentation. It remains experimental and needs
further human review and testing.

Jinra is not affiliated with or endorsed by Nintendo, Eden or the yuzu project.
The project is distributed under **GPL-3.0-or-later**; see [LICENSE.txt](LICENSE.txt)
and [LICENSES](LICENSES) for the retained license texts and notices.
