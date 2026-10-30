# Tabler icons — provenance

**Source:** <https://github.com/tabler/tabler-icons>, `icons/outline/`, taken 2026-10-30 (5,203 outlines, upstream
`main`). **Licence:** MIT — see `LICENSE` beside this file, copied verbatim from the repository root.

**What is here.** Every outline symbol Tabler publishes, at the three sizes this tree asks for:

| directory | contents |
|---|---|
| `24x24/actions/` | 5,203 PNGs, 24×24 px — the design size of a Tabler symbol |
| `48x48/actions/` | the same symbols at 48×48 px, for a 2× display |
| `96x96/actions/` | the same symbols at 96×96 px, for a 2× display at 48 points |
| `source/` | the 5,203 SVGs the pixels came from, and **not installed** |

Each PNG is black ink on transparency, which is what a *template* image is: the drawing side inks it, so a disabled
toolbar item is the grey of its own label. The theme is installed as an ordinary one under `share/icons/Tabler`, beside
`Frost`, and `libagora-iconservices` looks a name it cannot find in the theme in charge up in its sibling `Tabler` — that
is what makes `[NSImage imageNamed:@"hammer"]` work in a session whose theme is Frost.

**Why pre-rendered and not rendered by the build.** Forge learned this the hard way and this component inherits the
lesson: art produced by a *build step* is a property of how a project was built, so a project built by another program
(Forge building itself) silently got the drawn fallback while the checkout's build got the real thing. These pixels are
committed, so every build and every install produces the same ones, and no build needs `rsvg-convert`, ImageMagick or
Python to compile an application that uses them.

**How to regenerate.** `tools/render-tabler.py` is committed with the theme:

    python3 tools/render-tabler.py <tabler-icons>/icons/outline themes/Tabler 24,48,96

It runs `rsvg-convert` once per symbol per size and writes into `<size>x<size>/actions/`. The output is deterministic for
a given librsvg, so a regeneration that changes nothing produces no diff; a new upstream snapshot is a commit of its own,
with this file's date and count updated.

**What was not taken:** Tabler's filled set (1,054 symbols — a different visual language, and this tree's icons are
template outlines), the sprite/webfont builds, and the documentation site.
