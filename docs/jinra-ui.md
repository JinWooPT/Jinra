# Jinra desktop interface

Jinra is a local derivative of Eden-NS2/Eden/yuzu. Original copyright and
license notices are retained. The interface, controller artwork and NUP
integration were developed with AI assistance.

The home screen provides a searchable library with All games, Recent and
Favorites collections, a selected-game panel, graphics settings, controls
and system settings. Advanced settings remain available. The Jinra theme
uses compact settings dialogs and a flat runtime status bar.

Game artwork retains its original decoded pixels until it is painted. The
details panel avoids enlarging small embedded icons beyond their native
resolution. This cannot recover detail absent from the source game icon;
there is no automatic remote cover download.

The Controls page can preview Xbox and DualSense layouts. Automatic mode
uses SDL device metadata and recent meaningful input activity. It shows
the saved mappings for the selected player when a device is connected;
manual previews show a clearly marked standard position reference.
Virtual-controller tests cover Xbox and DualSense metadata and button
names; physical hardware still requires user verification. Input wrappers
that expose a virtual Xbox device are identified as that device.

New configurations default to English, the Jinra theme, Vulkan, 12 GB
memory, Handheld, 2x scaling, ScaleForce, SMAA, High GPU accuracy,
asynchronous shader building, GPU video decoding and aggressive VRAM
usage. Explicit existing preferences take precedence. Existing translation
catalogs and theme options remain available; new Jinra strings may use
English when a catalog does not yet contain a translation.

New initial accounts are named Jinra. Profiles without a custom avatar use
the approved Jinra logo in profile settings, the profile picker and the
emulated account service. The service embeds a 256x256 JPEG; the Qt
interface uses the original PNG for high-DPI rendering. Existing profile
names, identifiers, custom avatars and game save associations are kept.
The legacy storage and configuration key names remain for compatibility.

NUP support and its current limitations are documented in [nup-jinra.md](nup-jinra.md).
