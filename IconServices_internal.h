/*
 * IconServices_internal.h — what the two implementation files say to each other.
 *
 * Copyright © 2026 Kyle J. Cardoza. MIT licensed — see LICENSE.
 * SPDX-License-Identifier: MIT
 *
 * NOT INSTALLED AND NOT PUBLIC. `AGIcon.c` owns the icon object, its raster and the badge compositor;
 * `AGIconLoader.c` owns the loaders (`.icns`, PNG), the application-icon lookup and the drawn generics. The
 * seam is the representation list and one rasteriser.
 */
#ifndef ICONSERVICES_INTERNAL_H
#define ICONSERVICES_INTERNAL_H

#include "IconServices.h"

#include <CoreGraphics/CGColorSpace.h>
#include <CoreGraphics/CGContext.h>
#include <stdbool.h>
#include <stddef.h>

struct AGIcon {
	int refcount;
	CGImageRef *reps;
	size_t count;
	size_t cap;
};

/* A new, empty icon, +1. The loaders build one representation at a time. */
IconRef _AGIconCreateEmpty(void);

/* Add a representation, retaining the image. */
void _AGIconAddRepresentation(IconRef icon, CGImageRef image);

/* The representation best fitting `width` — a representation at least as wide is preferred to one smaller,
 * because scaling down is kinder than scaling up. NULL when the icon has none. */
CGImageRef _AGIconBestRepresentation(IconRef icon, CGFloat width);

/* A fresh bitmap context in this tree's pixel format — 8 bits per component, premultiplied, device RGB —
 * cleared to transparent, or NULL. The caller releases it. */
CGContextRef _AGIconCreateContext(size_t width, size_t height);

/* Draw `image` into a fresh context of `size` and return it as an image, +1. */
CGImageRef _AGIconRasterize(CGImageRef image, size_t width, size_t height);

/* ---------------------------------------------------------------------------------------------------
 * the loaders (AGIconLoader.c)
 * --------------------------------------------------------------------------------------------------- */

IconRef _AGIconCreateForURL(CFURLRef url);
IconRef _AGIconCreateGeneric(CFStringRef uti);

#endif /* ICONSERVICES_INTERNAL_H */
