# NUP support in Jinra

The NUP container and loader are adapted from Eglantine commit
`62214094d6c698b70f51016e5c12af415f025be7`, under GPL-3.0-or-later.
Original notices are retained. The NUP format and original implementation are
credited to a55uka and the Eglantine contributors.

- Source: https://github.com/Djihads80/Eglantine/tree/62214094d6c698b70f51016e5c12af415f025be7/src/core
- Format: https://github.com/a55uka/nup/blob/meow/SPEC.md

Jinra reads NUP v1 raw and Zstandard extents through its virtual filesystem.
The base program and newest bundled update are supported. Add-on records are
recognized, but mounting bundled DLC is not implemented. There is no automatic
game conversion or extraction to the host filesystem.

Additional local changes validate arithmetic and ranges before allocation,
check directory graphs and names, reject conflicting children, stream large
compressed members, bound decompressed caching and stream integrity checks.
The format has a draft specification; unsupported versions and codecs fail
with a loader error.

The project owner has successfully tested Donkey Kong Bananza from a `.nup`
dump. This is the only game validated with Jinra; other games, NUP layouts and
bundled DLC behavior are not covered by that result. Raw/Zstandard container
fixtures also passed 717 assertions across four test cases.

This integration and its tests were developed with AI assistance. No unrelated
GPU or CPU changes from Eglantine were imported.
