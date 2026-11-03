# Selective Eglantine correctness changes

## Defined floating-point min/max results

GPU-assisted validation observed `FMax` with two NaN operands in the Pokopia
fragment shaders. The SPIR-V backend now uses `NMin` and `NMax` for 32-bit guest
min/max operations. They preserve finite numeric results, select the numeric
operand when only one input is NaN, and return NaN when both are NaN. The
frontend's explicit NaN-propagating modifier remains separate.

The backend regression includes both-NaN, single-NaN, finite, infinity and
signed-zero input pairs with observable fragment outputs. This removes a
confirmed undefined SPIR-V result; it does not establish that the cinematic
crash is resolved. Primary semantics: [Khronos extended instructions](https://registry.khronos.org/SPIR-V/specs/unified1/GLSL.std.450.html)
and [NVIDIA PTX min/max](https://docs.nvidia.com/cuda/parallel-thread-execution/).

## Device-memory continuity

The map/unmap continuity correction is adapted from Eglantine commit
[`ddd367a8152f1c37bfc972475b40afece2da31ac`](https://github.com/Djihads80/Eglantine/commit/ddd367a8152f1c37bfc972475b40afece2da31ac),
by Takoroka, under GPL-3.0-or-later. Existing Eden and yuzu notices are retained.
The upstream change was inspected at Eglantine `nx2` commit
`46967a1b84e857a783ace6dcbabffddd008463ff`.

Remapping or unmapping device pages now ends earlier contiguous runs at the
changed range and resets the changed pages' run lengths. Reads and writes can
therefore no longer traverse a hole using the physical pages of an old mapping.
Jinra's CPU-backing probe and page-translation cache invalidation are preserved.

The regression tests construct valid initial mapping metadata and exercise the
real device-memory unmap and read/write paths, including holes at the start and
middle of a run, multi-page holes, preserved tail mappings, and cached reads.

## Proven predicate-mask normalization

The signed-maximum fold is selectively adapted from these GPL-3.0-or-later
Eglantine changes, retaining existing source notices:

- [`da7752585c4602d63194d62173629aa260fb9c9b`](https://github.com/Djihads80/Eglantine/commit/da7752585c4602d63194d62173629aa260fb9c9b),
  yumekiru: direct constant-select masks and the `SMax32` dispatch.
- [`38230873a4a59ad9c05651e95d2d76e0cc575228`](https://github.com/Djihads80/Eglantine/commit/38230873a4a59ad9c05651e95d2d76e0cc575228),
  vibecodekun: masks merged through arithmetic and Phi inputs.
- [`da17ced530d4032bc004645479b37cbd8cc71f6d`](https://github.com/Djihads80/Eglantine/commit/da17ced530d4032bc004645479b37cbd8cc71f6d),
  a55uka: separation and documentation of the mask-domain analysis.

Upstream reported an NVIDIA compiler fault for `max(x, -x)` in Mario Kart World.
The fold changes only exact integer expressions where the same resolved SSA
value is proved to be one of `-1`, `0`, or `1`. Direct selections normalize their
constant arms; other proved masks use `x & 1`. Unknown values retain their
original operations. The analysis has a depth limit of eight and an additional
conservative budget of 256 visited values per candidate. Literal subtraction
forms and unrelated runtime behavior are unchanged.

Pass-level tests cover all nine constant-select pairs, both maximum operand
orders, the merged Phi regression, arithmetic domain escapes, unknown inputs,
signed boundaries including `INT_MIN`, exact SSA identity, cycles, and both
analysis limits. An independent value evaluator checks numerical equivalence
and structural checks require proved maxima to disappear.

Vulkan and OpenGL cache versions remain 38 and 18. Their disk records contain
guest environments and pipeline keys, and both load paths call
`TranslateProgram` again before compiling the current output. This fold adds no
environment queries or serialized fields. Vulkan's opaque driver cache is also
used with newly created shader modules, whose code identifies compiled pipelines;
no old translated module is substituted by Jinra. A format-version bump is
therefore unnecessary for this optimization.

These integrations and tests were developed with AI assistance.

## Signed f32 to s32 saturation

The signed 32-bit conversion now clears NaN before converting and clamps the
floating-point operand to a representable range. The old upper bound rounded
`INT_MAX` to `2^31` in f32, so `OpConvertFToS` could receive a value outside
the signed 32-bit range. The translator selects `INT_MAX` explicitly when the
rounded input reaches that boundary. Both the Maxwell and Ampere frontends use
the same operation; other widths and unsigned conversions are unchanged.

The captured Pokopia compute shader uses this conversion. Its behavior follows
the [PTX `cvt` saturation rules](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#data-movement-and-conversion-instructions-cvt)
without relying on out-of-range [SPIR-V conversion behavior](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpConvertFToS).


## Pokopia cinematic on NVIDIA Vulkan

The verified compute shader elects one active lane to increment a shared
counter by the active lane count. Jinra substitutes an increment of one per
participating lane, preserving the total without using the final subgroup
ballot. The workaround is limited to the NVIDIA Vulkan driver and the exact
verified native compute module. The game reached the tutorial and accepted
movement with standard Vulkan on an RTX 5090.

The native GPU readback path applies pending CPU writes before copying GPU data
back to guest memory. This change is adapted from PokeDinner commit
[06e211e0d0a1](https://git.eden-emu.dev/Franseven/PokeDinner-a-Eglantine-Fork/commit/06e211e0d0a1634d16f1d74d75682092fc92a5de).
