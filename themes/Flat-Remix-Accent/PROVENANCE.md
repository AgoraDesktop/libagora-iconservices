# Flat-Remix-Accent — provenance

**FLAT-REMIX, RE-COLOURED AND PRE-RENDERED BY THIS TREE.** The desktop's default icon theme: places and file types,
looked up by a Freedesktop name (`folder`, `user-home`, `image-png`). It is the whole of `places/` and `mimetypes/` from
upstream, rendered at nine sizes and both scales by [`demo/flat-remix-accent.sh`](../../../demo/flat-remix-accent.sh),
with upstream's two folder tones replaced by Agora's accent.

| | |
|---|---|
| the theme | **Flat-Remix**, by Daniel Ruiz de Alegría (`daniruiz/flat-remix`) |
| the source | the upstream `master` archive (`flat-remix-master/`, gitignored beside this workspace), `places/scalable` and `mimetypes/scalable` only |
| what is committed | the RENDERED PNGs — nine sizes at 1× and 2× — plus upstream's `LICENSE` and this file. The SVGs are not committed: this stack has no SVG renderer, and `rsvg-convert` produces exactly these files |
| licence | **GPL-3.0** (upstream `LICENSE` beside the art). `libagora-iconservices` is GPL-3.0-or-later, so the terms compose; an LGPL or MIT consumer could not take this theme |
| the change Agora makes | the folder tones: upstream's `#8c42ab` becomes `NSColor`'s `+cg_accentColor` (164/154/255) and `#622e78` its 70% second tone — two `sed` substitutions over upstream's SVG, which is upstream's own recolouring recipe |
| what is NOT rendered | `apps/` and `actions/`. Application art is the bundle's own `.icns` (see `AGIconLoader.c`), and the UI's symbols are `Tabler`, installed beside this theme and asked for a name this one does not carry |
| how to regenerate | `demo/flat-remix-accent.sh` — it reads the upstream checkout, writes this directory, and prints where it put it |

**Why it is vendored rather than rendered on the machine.** A default theme that has to be built before the desktop has
icons is a default nobody sees. Frost was vendored for exactly that reason before it; this theme takes its place, and the
renderer stays in `demo/` as the recipe — the same split `Tabler` uses (pictures committed, `source/` not).

**The `index.theme` is this tree's**, written for the Freedesktop readers that expect one beside a theme directory; the
renderer does not write it, and `AGIconTheme.c` resolves by path and does not read it.
