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
and it is what reads the PNG representations inside an application's `.icns`.

```
$ readelf -d build/libagora-iconservices.so | grep NEEDED
  Shared library: [libagora-drawing.so]
  Shared library: [libagora-launchservices.so]
  Shared library: [libagora-core.so]
  Shared library: [libc.so.6]
```

**The model is Apple's.** An icon is looked up for an **item** (a URL), a **type** (a UTI), or as a
**generic**; it carries several **representations** at different sizes and a caller asks for the one for the
size it is about to draw. The generics are **drawn with Core Graphics, not shipped as assets**, so no binary
icon file is in this tree.

### What is measured, not assumed

`ctest` runs one suite, `tests/iconservices.c`, over a fixture application whose `.icns` **the test itself
composes** (two solid-colour PNGs embedded as base64 — the tree carries no binary asset). **16 checks, 0
fail**:

- an application's **declared** icon is found, and the pixels that come back are the ones the fixture gave
  each representation — **red at 32, blue at 128** — so the reader took the right representation, not just
  *a* representation;
- a size with no representation is **scaled to exactly that size**; the best size is the largest
  representation;
- the **generic** icons are made and drawn at the size asked; the text and folder generics **differ**, so a
  type really picks its own;
- a folder URL gets the folder generic;
- the **badge composite** keeps the base's size and differs from the unbadged icon;
- a **registered override** is what a type now draws, and it is no longer the built-in generic.

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
- **No SVG.** `nanosvg` (Zlib) is the named permissive engine; nothing needs it yet.
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
```

## Licence

MIT — see [`LICENSE`](LICENSE).
