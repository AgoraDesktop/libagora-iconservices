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
 * THE DEFAULT ART IS FROST. This library vendors the Frost icon theme (MIT, `themes/Frost`) and looks a type's
 * freedesktop name up there before it draws: `image/png` is `image-png`, a folder is `folder`, and a name the
 * theme does not carry falls back to the drawn generic. `AGORA_ICON_THEME` names another theme directory; it
 * is read at lookup time, and a path that resolves no icon simply leaves the drawing in charge.
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

IconRef AGIconRetain(IconRef icon);
void AGIconRelease(IconRef icon);

/* ---------------------------------------------------------------------------------------------------
 * AN AGORA-OWNED DOOR, named as Agora's and not as Apple's: Icon Services has no "replace the generic icon
 * for a type" entry point in its C API — the system's generic icons are resources. Agora's default art is
 * Frost, vendored rather than registered, but a caller that wants to substitute one for the process says so
 * here. `uti` and `icon` are retained.
 * --------------------------------------------------------------------------------------------------- */
void AGIconRegisterIconForType(CFStringRef uti, IconRef icon);

/* Drop every registered override and the theme's per-name cache. Agora-owned, and the reason is the same as
 * Launch Services': there is no Icon Services daemon to manage this state, so a caller that changed it says
 * when to forget. A change to `AGORA_ICON_THEME` also drops the cache at the next lookup. */
void AGIconReset(void);

#endif /* ICONSERVICES_ICONSERVICES_H */
