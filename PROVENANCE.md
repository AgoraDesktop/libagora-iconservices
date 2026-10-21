# Provenance

**First-party code, with one third-party asset: the Frost icon theme.** The implementation files are written
for Agora, not extracted from anywhere; the PNG decoder it relies on is Core Graphics', not its own. The art is
Frost's, and its licence and revision are recorded rather than folded into Agora's MIT.

| what | where |
|---|---|
| the guide | Apple's Icon Services model — an `IconRef` looked up for an item or a type, carrying several sized representations, with a badge compositor |
| the PNG decoder | **not this component's**: `CGImageCreateWithPNGDataProvider` in `libagora-drawing` (`CGImagePNG.c`), which links libpng |
| the `.icns` reader | **in-tree** (`AGIconLoader.c`), for the PNG-bearing entries |
| the default theme | **Frost v0.3.1** by Sayantan, MIT — see [`themes/PROVENANCE.md`](themes/PROVENANCE.md) |
| third-party code | **none**; the theme is data, not linked code |
| licence | MIT, per file (`LICENSE`), with Frost's own `LICENSE` beside its art |

## What was checked, and what was decided

**The doors are Agora's and the reason is Apple's Carbon.** Apple's Icon Services is fetched through
`GetIconRefFromFileInfo(const FSRef *, …)` or `GetIconRefFromTypeInfo(OSType, …)`, and an `FSRef` is exactly
what this tree does not have. The handle keeps Apple's name — `IconRef` — and the raster that comes out is a
`CGImageRef`, Core Graphics' own type. The only thing dropped is the Carbon parameter, and it is stated at the
door in `IconServices.h`.

**A PNG is not decoded here.** `libagora-drawing` already exposes `CGImageCreateWithPNGDataProvider`, and its
header says the decoder is libpng, reached through the drawing layer. So this component links no decoder of
its own and rode a link flag that already existed — the same reason `../WORKSPACE-PLAN.md` §2 named libpng
*for the drawing layer*, not for this one.

**The drawn generics remain the fallback.** Apple's generic icons are resources. Agora used to draw its own
because it shipped no art; with Frost vendored, the drawn document, folder, application, image, text, movie,
audio and archive generics are what a missing theme, a missing name or a deliberately empty `AGORA_ICON_THEME`
falls back to. They are still at 256 points, deterministic and reviewable, and they are no longer the default
appearance.

**The vendored theme is a release artifact, not a build dependency.** Frost authors SVG and ships PNG at every
size and both scales because SVG filters render differently across toolkits (its ADR-001). Agora has no SVG
renderer, so committing the built PNGs is exactly the form this stack can read; bringing the SVG sources would
bring Bun, `resvg` and `sharp` with them for no visible gain. The archive's tag, commit and SHA-256 are pinned
in `themes/PROVENANCE.md`, and the generated `icon-theme.cache` was removed from the copy.

**The `.icns` reader is partial, and says so.** The PNG-bearing entries are read and identified by the PNG
signature rather than a type table; the legacy RLE/ARGB entries are not read yet. This is recorded in
`README.md` and at the top of `AGIconLoader.c` rather than left to be discovered.

## Licence per file

Every implementation file is MIT. The art under `themes/Frost/` is MIT too, under Frost's own copyright notice
(`themes/Frost/LICENSE`); it is recorded separately because it travels separately, not because the terms differ.
