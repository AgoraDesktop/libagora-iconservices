# Provenance — the vendored icon theme

**ONE THEME LIVES HERE NOW: `Tabler/`, the UI's symbols.** Every outline Tabler publishes, pre-rendered and looked up by
NAME; it is asked for a name the theme in charge does not carry, which is why it is owed as a sibling rather than merged
into anything. Its source, licence, sizes and regenerating command are in [`Tabler/PROVENANCE.md`](Tabler/PROVENANCE.md).

**`Flat-Remix-Accent/` IS THE DESKTOP'S OWN SET**, and it is vendored as its rendered PNGs — the theme
`demo/flat-remix-accent.sh` produces, installed to `share/icons` and named by the compiled default. Its source, the two
colour substitutions that make it Agora's, and the command that regenerates it are in
[`Flat-Remix-Accent/PROVENANCE.md`](Flat-Remix-Accent/PROVENANCE.md).

**`Frost/` WAS HERE AND IS GONE.** It was the same seat — places, file types and application art, looked up by type and
the theme in charge by default — vendored from **Frost v0.3.1** by Sayantan (MIT, archive SHA-256
`de4b95d10e2720c36320975ec2dc25f9113155783338648f9e50837b3fcdc3d9`, commit
`84e3f8733dd20e45d9ebebc84c0eca996cbc9f4a`). The desktop's default moved to Flat-Remix, and 41 MB of art for a theme
nothing runs was the wrong thing to keep. This file keeps the record so the removal can be read for what it was.

## What was decided

**THE COMPONENT SHIPS NO PLACES ART, AND NEEDS NONE OF ITS OWN.** A theme is data: `AGORA_ICON_DEFAULT_THEME` names a
directory in the install prefix and the lookup reads PNGs from it. Whatever desktop art a machine runs is that machine's
to render — the accent-tinted Flat-Remix the demo script produces, a distribution theme, or nothing, in which case the
drawn generics in `AGIconLoader.c` answer.

**THE BUILT RELEASE IS VENDORED, NOT THE SVG SOURCES.** For `Tabler`, the PNGs are the form this stack reads: Agora has
no SVG renderer, and bringing one would add a renderer and an icon build toolchain for no visible gain. The generated
`icon-theme.cache`, if any, is not committed.
