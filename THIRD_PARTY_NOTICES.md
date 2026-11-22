# Third-party and research-asset provenance

The MIT LICENSE is intended for original project-owned source code and documentation only. It does not override or relicense independently copyrighted third-party software, fonts, graphics, datasets, models, presentations or referenced scientific works.

## Native build dependencies

- GLFW — third-party native window/input library, with its own upstream license. CMake uses the system-installed library rather than granting any new rights to GLFW.
- OpenGL and the platform graphics/system frameworks — platform APIs/libraries subject to their own licensing, redistribution, and vendor rules. On macOS the native viewer uses CoreText/CoreGraphics/CoreFoundation.
- Bullet Physics / btDbvtBroadphase — external independent comparator when installed. Its license and notices come from upstream Bullet; this repository's MIT LICENSE does **not** apply to Bullet.
- Python and C++ toolchains, including standard-library and system packages — individually licensed by their maintainers. Tools referenced in reproduction instructions are not embedded rights transfers.

## Research evidence and publication files

Generated images, charts, tables and benchmark output are evidence tied to a source revision, fixture, machine and method. Reusing them in a publication requires preserving data provenance and checking asset/figure rights. They do not certify unpublished novelty or general algorithm superiority.

Any file imported or copied from third parties must retain the source attribution, upstream license and modification history. Do not assume a file is project-owned solely because it is stored in this repository.

## Publishing guidance

Before distributing a packaged binary or teaching archive, inventory every linked/bundled dependency and copied asset; include each applicable upstream notice. Do not redistribute proprietary datasets or external binaries under this repository's MIT grant.

This note describes scope; the LICENSE file contains the actual grant for material the copyright holder is authorized to license.
