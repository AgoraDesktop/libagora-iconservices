# Provenance

**First-party code, with one third-party asset: the Tabler symbol theme.** The implementation files are written
for Agora, not extracted from anywhere; the PNG decoder it relies on is Core Graphics', not its own. The art is
Tabler's, and its licence and revision are recorded in its own provenance rather than folded into Agora's MIT.

| what | where |
|---|---|
| the guide | Apple's Icon Services model — an `IconRef` looked up for an item or a type, carrying several sized representations, with a badge compositor |
| the PNG decoder | **not this component's**: `CGImageCreateWithPNGDataProvider` in `libagora-drawing` (`CGImagePNG.c`), which links libpng |
| the `.icns` reader | **in-tree** (`AGIconLoader.c`), for the PNG-bearing entries |
| the default theme | **Flat-Remix-Accent**, rendered per machine by `demo/flat-remix-accent.sh` (GPL-3.0) — named, never vendored |
| the vendored art | **Tabler**, MIT — see [`themes/Tabler/PROVENANCE.md`](themes/Tabler/PROVENANCE.md) |
| third-party code | **none**; the theme is data, not linked code |
| licence | MIT, per file (`LICENSE`), with Tabler's own `LICENSE` beside its art |

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
because it shipped no art; now that the default theme is a rendered one, the drawn document, folder, application,
image, text, movie, audio and archive generics are what a missing theme, a missing name or a deliberately empty
`AGORA_ICON_THEME` falls back to — which includes every machine that has not run `demo/flat-remix-accent.sh`. They are
still at 256 points, deterministic and reviewable.

**The default theme is not vendored any more, and that is a licence decision.** Flat-Remix is GPL-3.0: its sources and
the PNGs rendered from them are produced per machine by `demo/flat-remix-accent.sh`, and the compiled default names the
directory that script writes. What this component commits is `Tabler` alone — MIT, and the UI's symbols rather than a
desktop's places art. The theme this component used to vendor, Frost (MIT v0.3.1), was removed with its install rule;
`themes/PROVENANCE.md` records what it was and why nothing replaces it in the tree.

**The `.icns` reader is partial, and says so.** The PNG-bearing entries are read and identified by the PNG
signature rather than a type table; the legacy RLE/ARGB entries are not read yet. This is recorded in
`README.md` and at the top of `AGIconLoader.c` rather than left to be discovered.

## Licence per file

Every implementation file is MIT. The art under `themes/Tabler/` is MIT too, under Tabler's own copyright notice
(`themes/Tabler/LICENSE`); it is recorded separately because it travels separately, not because the terms differ.
