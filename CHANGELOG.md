# Changelog

All notable changes to Eglantine are documented here. Each release's notes are taken from its
`## [X.Y.Z]` section (release candidates use the section of the version they are a candidate for).
New entries go under `## [Unreleased]`; `tools/ci/bump-version.sh` turns them into a version section.

## [Unreleased]

## [1.1.1]

- Reuse Vulkan views for single-layer render targets and avoid duplicate reads on linear texture uploads.
- Add optional Ocarina of Time high-refresh timing and timestamp-based Switch 2 video playback.
- Use a compute fallback for task and mesh draws on affected AMD Vulkan drivers.
- Make the per-game Graphics tab scrollable and report the Jinra version in Windows file properties.
- Add optional timings for texture uploads and intervals between Zelda frame submissions.

## [0.1.0]

- Initial release candidate
