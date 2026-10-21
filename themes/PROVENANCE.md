# Provenance — the vendored icon theme

**The one third-party asset in this component, and the reason it is here.** Agora's generic icons used to be
drawn by `AGIconLoader.c` because the tree shipped no art. That is still the fallback, but the interim
**default icon set is Frost**, and Frost is vendored rather than installed by hand so a fresh checkout gets the
same desktop.

| what | where |
|---|---|
| the theme | **Frost — a frosted-glass icon theme for Linux**, by Sayantan (`thissayantan/frost-icon-theme`) |
| upstream tag | **v0.3.1**, commit `84e3f8733dd20e45d9ebebc84c0eca996cbc9f4a` |
| the artifact | the release archive `Frost-v0.3.1.tar.xz`, SHA-256 `de4b95d10e2720c36320975ec2dc25f9113155783338648f9e50837b3fcdc3d9` |
| what is committed | the archive's `Frost/` directory, verbatim except for `icon-theme.cache` (a generated, stale index), plus the release's `LICENSE` beside the art |
| licence | **MIT**, upstream `LICENSE` beside the art in `Frost/LICENSE` |
| third-party code | none: an icon theme is data, and this component compiles none of it |

## What was decided

**THE BUILT RELEASE IS VENDORED, NOT THE SVG SOURCES.** Frost authors SVG and ships pre-rendered PNG at every
size and both scales, because SVG filters render differently in GTK, Qt and the shell (its ADR-001). Agora has
no SVG renderer and does not need one for this: `AGIconTheme.c` reads the PNG representations that Core
Graphics already decodes. Vendoring the sources would add Frost's Bun/TypeScript toolchain, `resvg` and `sharp`
to Agora's build for no user-visible gain.

**IT IS AN INTERIM DEFAULT, AND IT IS A DEFAULT RATHER THAN A DEPENDENCY.** `AGORA_ICON_THEME` points the
process at another theme; a theme that does not resolve falls back to the generics `AGIconLoader.c` draws. The
art is MIT, so it can be redistributed with Agora — unlike the GPL/CC-BY-SA window-chrome theme the AppKit
reads at run time and therefore does not ship.

**THE CACHE FILE IS NOT COMMITTED.** `icon-theme.cache` is a generated lookup index; it is architecture- and
revision-dependent and belongs to whichever tool builds it, not to the vendored copy.

## Updating it

Take the `Frost-vX.Y.Z.tar.xz` asset from the upstream release, verify the archive, unpack `Frost/` over
`Frost/`, remove `icon-theme.cache`, copy the release's `LICENSE` to `Frost/LICENSE`, and record the new tag,
commit and SHA-256 here. The theme's own `index.theme` carries its version in `Comment`.
