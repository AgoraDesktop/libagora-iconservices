# libagora-iconservices

**Icon Services for Agora** — the icon an item or a type is drawn with: the lookup, the representations at
different sizes, and the badge compositor.

Agora is an installable desktop environment for Linux and the BSDs; see the project README in the parent
directory (`../README.md`). `libagora-iconservices` sits above Core Graphics (a raster comes out as a
`CGImageRef`) and calls across to `libagora-launchservices` for the type model; the AppKit's
`NSWorkspace -iconForFile:` is that layer's face over this one.

## Provenance

Written for Agora, against Apple's Icon Services model — not extracted from anywhere, and linking nothing
foreign. See [`PROVENANCE.md`](PROVENANCE.md).

## Status

**Built, C throughout, and measured.** The library links the tree's own Core Graphics, Launch Services and
Core Foundation, and libc — **no third-party library of its own**, because a PNG is decoded by Core Graphics:
`CGImageCreateWithPNGDataProvider` already turns PNG bytes into an image (`libagora-drawing`, `CGImagePNG.c`),
and it is what reads the PNG representations inside an application's `.icns` and inside the vendored theme.

```
$ readelf -d build/libagora-iconservices.so | grep NEEDED
  Shared library: [libagora-drawing.so]
  Shared library: [libagora-launchservices.so]
  Shared library: [libagora-core.so]
  Shared library: [libc.so.6]
```

**The model is Apple's.** An icon is looked up for an **item** (a URL), a **type** (a UTI), or as a
**generic**; it carries several **representations** at different sizes and a caller asks for the one for the
size it is about to draw.

## The default icon set: Flat-Remix-Accent

**Agora's default art is Flat-Remix**, in the accent-tinted variant `demo/flat-remix-accent.sh` renders from the
upstream checkout: folders, file types and the places icons a file manager asks for, with the folder tones swapped to
`NSColor`'s `+cg_accentColor`. It is **GPL-3.0** — as this component is — and it is vendored at
`themes/Flat-Remix-Accent` as its rendered PNGs (nine sizes, both scales, `places` and `mimetypes`), installed to
`share/icons`, and named by the compiled default (`AGORA_ICON_DEFAULT_THEME`). `themes/Flat-Remix-Accent/PROVENANCE.md`
records the source, the two colour substitutions and the command that regenerates it.

The theme this component used to vendor — Frost (MIT, v0.3.1) — is gone, along with its 41 MB of PNGs. `themes/` holds
two themes now: this one, and `Tabler`, the UI's outline symbols, which installs beside it and answers a name it does not
carry.

A type's name is its preferred MIME tag with `/` turned into `-` (`image/png` → `image-png`); a type with no
MIME tag falls back to the generic for its category (`text-x-generic`, `folder`, `application-x-executable`).
A directory gets its place's icon where the theme has one — `user-home`, `user-desktop`, `folder-documents`,
`user-trash` — and `folder` otherwise. Both scales are read, so a 512-pixel request is the 256 @2x art rather
than a scaled-up 256.

**It is a default, not a dependency.** `AGORA_ICON_THEME` points the process at another theme directory; a
theme that does not resolve falls back to the **generics drawn with Core Graphics**, so a checkout without the
theme still draws something for every item. `AGIconReset` drops the override registry and the per-name theme
cache; changing `AGORA_ICON_THEME` drops the cache too.

### What is measured, not assumed

`ctest` runs one suite, `tests/iconservices.c`, over a fixture application whose `.icns` **the test itself
composes** (two solid-colour PNGs embedded as base64 — the test carries no binary asset). **24 checks, 0
fail**:

- an application's **declared** icon is found, and the pixels that come back are the ones the fixture gave
  each representation — **red at 32, blue at 128** — so the reader took the right representation, not just
  *a* representation;
- a size with no representation is **scaled to exactly that size**; the best size is the largest
  representation;
- the **icons** are made at the size asked; the text and folder icons **differ**, so a type really picks
  its own;
- a folder URL gets the folder icon;
- the **badge composite** keeps the base's size and differs from the unbadged icon;
- a **registered override** is what a type now draws, and it is no longer the built-in generic;
- the **vendored theme** resolves `folder` at 32 and at 512 pixels (the 256 @2x art), a PNG gets its own
  `image-png` rather than the image generic, and pointing `AGORA_ICON_THEME` at an empty directory brings the
  drawn 256-point generic back — so the theme is measured as a default over the drawing, not as a claim beside it.

## The doors

`IconServices.h` — `AGIconCreateWithURL` (an item), `AGIconCreateWithType` (a type), `AGIconCreateWithImage`,
`AGIconCreateWithBadge` (compositor), `AGIconCopyImage` (the raster for a drawing size),
`AGIconGetBestSize`, `AGIconRetain`/`AGIconRelease`, and the Agora-owned `AGIconRegisterIconForType` /
`AGIconReset`.

### What is deliberately not here

- **The legacy `.icns` entries are not read.** The modern PNG-bearing entries (ic07–ic14) are; the older
  RLE/ARGB forms (`is32`, `il32`, `ih32`, `it32`, `ic04`, `ic05`) are named and not read yet, so an `.icns`
  holding only those yields no icon and the caller falls back to the generic. (libicns is the named LGPL
  fallback if this ever becomes worth linking.)
- **No SVG.** `nanosvg` (Zlib) is the named permissive engine; nothing needs it. The one theme this component still
  vendors, `Tabler`, is committed as its built PNGs for the same reason — the SVG sources are upstream's, not this
  tree's.
- **No asset catalogues** (`CFBundleIconName`); only `CFBundleIconFile`.
- **No writing `.icns`**, and no custom-icon store. Custom icons are a Workspace metadata concern
  (`../WORKSPACE-PLAN.md` §5.4), not Icon Services'.
- **No icon *for* a running application** in the Dock sense; that is the Dock's own concern.

## Build and run

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=/usr/bin/clang-19 \
      -DCMAKE_PREFIX_PATH=../build-prefix -DCMAKE_INSTALL_PREFIX=../build-prefix
cmake --build build
cd build && ctest --output-on-failure
cmake --install .      # installs share/icons/Flat-Remix-Accent and share/icons/Tabler
```

`AGORA_ICON_THEME=/path/to/ATheme` overrides the installed default at run time; without it the compiled default names
`share/icons/Flat-Remix-Accent`, which this component installs. **The suite needs neither**:
it composes a small theme of its own — an `index.theme` and a few flat-coloured PNGs, with the checkout's `Tabler`
linked beside it — so what it measures is the lookup rather than anybody's art.

## Licence

MIT — see [`LICENSE`](LICENSE).
