/*
 * IconServices.h — the icon an item or a type is drawn with.
 *
 * Copyright © 2026 Kyle J. Cardoza.
 * SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * THE MODEL IS APPLE'S (WORKSPACE-PLAN.md §2, F2). An icon is looked up for an ITEM (a URL), for a TYPE (a
 * UTI), or as a GENERIC fallback; it carries several REPRESENTATIONS at different sizes, and a caller asks
 * for the best one for the size it is about to draw — which is Icon Services' own shape. A badge composites
 * over a base icon.
 *
 * THE DOORS ARE AGORA'S, AND WHY IS AT THE DOOR. Apple's Icon Services is Carbon-shaped: `IconRef` is
 * fetched by `GetIconRefFromFileInfo(const FSRef *, …)` or `GetIconRefFromTypeInfo(OSType, …)`, and an
 * `FSRef` is exactly what this tree does not have. The ICON HANDLE keeps Apple's name (`IconRef`) and the
 * raster that comes out is a `CGImageRef`, which is Core Graphics' type on both systems. The one thing
 * dropped is the Carbon parameter, and it is stated rather than silently changed.
 *
 * A PNG IS DECODED BY CORE GRAPHICS, not by this library: `CGImageCreateWithPNGDataProvider` already turns
 * PNG bytes into an image (`libagora-drawing`, `CGImagePNG.c`), so an application's `.icns` and any PNG
 * icon ride the drawing layer and this component links no decoder of its own.
 *
 * THE DEFAULT ART IS A THEME IN THE PREFIX, NAMED AND NOT VENDORED. `AGORA_ICON_DEFAULT_THEME` points at
 * `share/icons/Flat-Remix-Accent`, which `demo/flat-remix-accent.sh` renders from the user's own Flat-Remix checkout
 * (GPL-3.0, so nothing of it is committed or installed by this build). A type's freedesktop name is looked up there
 * before anything is drawn — `image/png` is `image-png`, a folder is `folder` — and a name the theme does not carry
 * falls back to the drawn generic. `AGORA_ICON_THEME` names another theme directory; it is read at lookup time, and a
 * path that resolves no icon simply leaves the drawing in charge, which is what a machine without the theme gets.
 *
 * **AND THE UI'S SYMBOLS ARE A SECOND THEME, WHICH IS ASKED WHEN THE FIRST ONE MISSES.** `themes/Tabler` is every
 * outline symbol Tabler publishes — 5,203 of them, pre-rendered at 24, 48 and 96 pixels — installed beside the theme in
 * charge under `share/icons`, and a name that theme does not carry is looked up in its sibling `Tabler` before the answer
 * is no. That is what makes a symbol a SYSTEM RESOURCE: `AGIconCreateWithName(CFSTR("hammer"))` — and through it AppKit's
 * `+[NSImage imageNamed:]` — answers the vendored artwork in any application, instead of every toolbar compiling its own
 * copy of the pictures it wants. The two are not merged: the theme in charge is places, file types and application art,
 * Tabler is the UI's verbs and switches, and the lookup keeps them apart while making both reachable.
 */
#ifndef ICONSERVICES_ICONSERVICES_H
#define ICONSERVICES_ICONSERVICES_H

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGGeometry.h>
#include <CoreGraphics/CGImage.h>

/* AN ICON IS AN OPAQUE HANDLE, which is Apple's `IconRef`. It is refcounted; every `AGIconCreate…` returns
 * +1 and the caller `AGIconRelease`s it. */
typedef struct AGIcon *IconRef;

/* THE ICON FOR AN ITEM. An application bundle's own icon is read from its `Info.plist`
 * (`CFBundleIconFile`); a directory gets its place's theme icon (`user-home`, `user-desktop`, `folder`, …);
 * anything else falls back to the icon for its type. Never NULL for a URL: an item always has something to
 * draw. */
IconRef AGIconCreateWithURL(CFURLRef url);

/* THE ICON FOR A TYPE, chosen by conformance — an image, a text document, a folder, an application. The
 * vendored theme's icon for the type's MIME name wins; its category generic or a drawn generic is the
 * fallback. An override registered with `AGIconRegisterIconForType` wins over both. */
IconRef AGIconCreateWithType(CFStringRef uti);

/* **THE NAME ITSELF, WHICH IS WHAT A TOOLBAR ASKS FOR.** `AGIconCreateWithType` wants a UTI, and a caller that means
 * "the symbol called `hammer`" has no type to give. This door looks the name up in the installed themes — the one
 * `AGORA_ICON_THEME` names, then its sibling `Tabler` — and answers NULL for a name no theme carries, which leaves the
 * caller's own drawing in charge. AppKit's `+[NSImage imageNamed:]` is the caller that makes this a system resource.
 *
 * **AND A NAME MAY SAY WHICH THEME IT MEANS: `toolbar:folder-open`, `tabler:folder-open`, `flat-remix:folder-open`.**
 * Two themes can carry one name and mean two different pictures — the desktop theme's `folder-open` is a filled,
 * coloured place folder and Tabler's is an outline symbol — so the prefix settles it, matched without case against the
 * theme directories beside the one in charge. `toolbar:` is the ALIAS for the theme that holds the UI's symbols, which is
 * what a band wants and what lets the theme in charge be replaced without every caller changing; `tabler:` and a theme's
 * own directory name (mapped through the aliases in `AGIconTheme.c`) name one directly. A prefix that names no theme is
 * not a guess: nothing answers, exactly as for a name no theme carries. AN UNADORNED NAME ASKS THE THEME IN CHARGE — the
 * default, unless `AGORA_ICON_THEME` says otherwise — and then its sibling. */
IconRef AGIconCreateWithName(CFStringRef name);

/* WRAP AN IMAGE THE CALLER ALREADY HAS. */
IconRef AGIconCreateWithImage(CGImageRef image);

/* THE BADGE COMPOSITOR: `badge` is drawn at `fraction` of the base icon's side, in its bottom-right
 * corner, over every representation the base has. */
IconRef AGIconCreateWithBadge(IconRef base, IconRef badge, CGFloat fraction);

/* THE RASTER FOR A DRAWING SIZE: the best representation, scaled to exactly `size` if it does not already
 * match. `size` of 0 in a dimension means "the icon's own best size" in that dimension. Returns +1. */
CGImageRef AGIconCopyImage(IconRef icon, CGSize size);

/* THE LARGEST SIZE THE ICON HAS A REPRESENTATION FOR. */
CGSize AGIconGetBestSize(IconRef icon);

/* **AND THE SMALLEST, WHICH IS THE ARTWORK'S OWN DESIGN SIZE.** A Tabler symbol is drawn on a 24-point grid and ships at
 * 24, 48 and 96 pixels; a caller making an `NSImage` needs to know what the icon is *for* rather than how big its
 * largest painting is. */
CGSize AGIconGetNaturalSize(IconRef icon);

IconRef AGIconRetain(IconRef icon);
void AGIconRelease(IconRef icon);

/* ---------------------------------------------------------------------------------------------------
 * AN AGORA-OWNED DOOR, named as Agora's and not as Apple's: Icon Services has no "replace the generic icon
 * for a type" entry point in its C API — the system's generic icons are resources. Agora's default art is a named
 * theme rather than a registered icon, but a caller that wants to substitute one for the process says so here. `uti` and `icon` are retained.
 * --------------------------------------------------------------------------------------------------- */
void AGIconRegisterIconForType(CFStringRef uti, IconRef icon);

/* Drop every registered override and the theme's per-name cache. Agora-owned, and the reason is the same as
 * Launch Services': there is no Icon Services daemon to manage this state, so a caller that changed it says
 * when to forget. A change to `AGORA_ICON_THEME` also drops the cache at the next lookup. */
void AGIconReset(void);

#endif /* ICONSERVICES_ICONSERVICES_H */
