# Provenance

**First-party: written for Agora, not extracted from anywhere.** No upstream revision to pin and no
third-party code to licence — the PNG decoder it relies on is Core Graphics', not its own.

| what | where |
|---|---|
| the guide | Apple's Icon Services model — an `IconRef` looked up for an item or a type, carrying several sized representations, with a badge compositor |
| the PNG decoder | **not this component's**: `CGImageCreateWithPNGDataProvider` in `libagora-drawing` (`CGImagePNG.c`), which links libpng |
| the `.icns` reader | **in-tree** (`AGIconLoader.c`), for the PNG-bearing entries |
| third-party code | **none** |
| licence | MIT, per file (`LICENSE`) |

## What was checked, and what was decided

**The doors are Agora's and the reason is Apple's Carbon.** Apple's Icon Services is fetched through
`GetIconRefFromFileInfo(const FSRef *, …)` or `GetIconRefFromTypeInfo(OSType, …)`, and an `FSRef` is exactly
what this tree does not have. The handle keeps Apple's name — `IconRef` — and the raster that comes out is a
`CGImageRef`, Core Graphics' own type. The only thing dropped is the Carbon parameter, and it is stated at
the door in `IconServices.h`.

**A PNG is not decoded here.** `libagora-drawing` already exposes `CGImageCreateWithPNGDataProvider`, and its
header says the decoder is libpng, reached through the drawing layer. So this component links no decoder of
its own and rode a link flag that already existed — the same reason `../WORKSPACE-PLAN.md` §2 named libpng
*for the drawing layer*, not for this one.

**The generics are drawn, not shipped.** Apple's generic icons are resources. Agora ships none, so the
document, folder, application, image, text, movie, audio and archive generics are drawn with Core Graphics at
256 points — deterministic and reviewable, with no binary asset in the repository.

**The `.icns` reader is partial, and says so.** The PNG-bearing entries are read and identified by the PNG
signature rather than a type table; the legacy RLE/ARGB entries are not read yet. This is recorded in
`README.md` and at the top of `AGIconLoader.c` rather than left to be discovered.

## Licence per file

Every file is MIT. There is no third-party code in this component, so there is no per-file licence split to
record.
