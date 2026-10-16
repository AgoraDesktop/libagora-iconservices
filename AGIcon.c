/*
 * AGIcon.c — the icon object: its representations, the raster for a drawing size, and the badge compositor.
 *
 * Copyright © 2026 Kyle J. Cardoza. MIT licensed — see LICENSE.
 * SPDX-License-Identifier: MIT
 *
 * AN ICON IS A LIST OF REPRESENTATIONS, which is Icon Services' own shape: the same icon drawn at several
 * sizes, with the caller picking the one for the size it is about to draw. When the size asked for is not a
 * representation's, the best one is SCALED — down in preference to up, because scaling a larger
 * representation down loses less than blowing a smaller one up.
 *
 * EVERY PIXEL HERE IS THIS TREE'S OWN FORMAT, `kCGImageAlphaPremultipliedFirst` with
 * `kCGBitmapByteOrder32Little` in device RGB — the one `libagora-drawing`'s `CGBitmapContextCreate` pins and
 * the one `CGImageCreateWithPNGDataProvider` produces. Nothing converts between formats because there is
 * only one.
 */
#include "IconServices_internal.h"

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGColor.h>
#include <stdlib.h>
#include <string.h>

static IconRef iconAlloc(void)
{
	IconRef icon = calloc(1, sizeof(*icon));
	if (icon != NULL) icon->refcount = 1;
	return icon;
}

/* Called by the loaders, which build an icon representation by representation. */
IconRef _AGIconCreateEmpty(void) { return iconAlloc(); }

void _AGIconAddRepresentation(IconRef icon, CGImageRef image)
{
	if (icon == NULL || image == NULL) return;
	if (icon->count == icon->cap) {
		icon->cap = icon->cap ? icon->cap * 2 : 4;
		icon->reps = realloc(icon->reps, icon->cap * sizeof(*icon->reps));
	}
	icon->reps[icon->count++] = CGImageRetain(image);
}

IconRef AGIconRetain(IconRef icon)
{
	if (icon != NULL) icon->refcount++;
	return icon;
}

void AGIconRelease(IconRef icon)
{
	if (icon == NULL) return;
	if (--icon->refcount > 0) return;
	for (size_t i = 0; i < icon->count; i++) CGImageRelease(icon->reps[i]);
	free(icon->reps);
	free(icon);
}

CGContextRef _AGIconCreateContext(size_t width, size_t height)
{
	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	if (space == NULL) return NULL;
	CGContextRef ctx = CGBitmapContextCreate(NULL, width, height, 8, width * 4, space,
	                                         kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
	CGColorSpaceRelease(space);
	if (ctx == NULL) return NULL;
	memset(CGBitmapContextGetData(ctx), 0, width * 4 * height);	/* transparent, deterministically */
	return ctx;
}

CGImageRef _AGIconRasterize(CGImageRef image, size_t width, size_t height)
{
	CGContextRef ctx = _AGIconCreateContext(width, height);
	if (ctx == NULL) return NULL;
	CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)width, (CGFloat)height), image);
	CGImageRef out = CGBitmapContextCreateImage(ctx);
	CGContextRelease(ctx);
	return out;
}

CGImageRef _AGIconBestRepresentation(IconRef icon, CGFloat width)
{
	if (icon == NULL || icon->count == 0) return NULL;
	if (width <= 0) {					/* no size asked for: the largest */
		CGImageRef best = icon->reps[0];
		for (size_t i = 1; i < icon->count; i++)
			if (CGImageGetWidth(icon->reps[i]) > CGImageGetWidth(best)) best = icon->reps[i];
		return best;
	}
	CGImageRef best = icon->reps[0];
	double bestScore = 1e18;
	for (size_t i = 0; i < icon->count; i++) {
		double w = (double)CGImageGetWidth(icon->reps[i]);
		double score = (w >= width) ? (w - width) : ((width - w) + 1000.0);	/* under-size is penalised */
		if (score < bestScore) { bestScore = score; best = icon->reps[i]; }
	}
	return best;
}

IconRef AGIconCreateWithImage(CGImageRef image)
{
	if (image == NULL) return NULL;
	IconRef icon = iconAlloc();
	_AGIconAddRepresentation(icon, image);
	return icon;
}

CGSize AGIconGetBestSize(IconRef icon)
{
	CGSize size = CGSizeMake(0, 0);
	if (icon == NULL) return size;
	for (size_t i = 0; i < icon->count; i++) {
		CGFloat w = (CGFloat)CGImageGetWidth(icon->reps[i]);
		if (w > size.width) { size.width = w; size.height = (CGFloat)CGImageGetHeight(icon->reps[i]); }
	}
	return size;
}

CGImageRef AGIconCopyImage(IconRef icon, CGSize size)
{
	if (icon == NULL) return NULL;
	CGImageRef rep = _AGIconBestRepresentation(icon, size.width);
	if (rep == NULL) return NULL;

	size_t rw = CGImageGetWidth(rep), rh = CGImageGetHeight(rep);
	size_t tw = size.width  > 0 ? (size_t)size.width  : rw;
	size_t th = size.height > 0 ? (size_t)size.height
	                           : (size.width > 0 ? (size_t)((double)rh * (double)tw / (double)rw) : rh);
	if (tw == rw && th == rh) return CGImageRetain(rep);
	return _AGIconRasterize(rep, tw, th);
}

IconRef AGIconCreateWithBadge(IconRef base, IconRef badge, CGFloat fraction)
{
	if (base == NULL || badge == NULL) return NULL;
	IconRef out = iconAlloc();
	for (size_t i = 0; i < base->count; i++) {
		CGImageRef rep = base->reps[i];
		size_t w = CGImageGetWidth(rep), h = CGImageGetHeight(rep);
		CGContextRef ctx = _AGIconCreateContext(w, h);
		if (ctx == NULL) continue;
		CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), rep);
		CGFloat side = fraction * (CGFloat)(w < h ? w : h);
		if (side >= 1) {
			CGImageRef br = _AGIconBestRepresentation(badge, side);
			if (br != NULL) CGContextDrawImage(ctx, CGRectMake((CGFloat)w - side, 0, side, side), br);
		}
		CGImageRef image = CGBitmapContextCreateImage(ctx);
		CGContextRelease(ctx);
		if (image != NULL) { _AGIconAddRepresentation(out, image); CGImageRelease(image); }
	}
	return out;
}
