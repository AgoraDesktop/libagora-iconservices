/*
 * AGIcon.c — the icon object: its representations, the raster for a drawing size, and the badge compositor.
 *
 * Copyright © 2026 Kyle J. Cardoza.
 * SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
 * SPDX-License-Identifier: LGPL-3.0-or-later
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

/* ONE PASS DOWN, WITH THE SAMPLER ON. This tree's CG defaults to `kCGInterpolationNone`, so a draw that SHRINKS a
 * picture takes one source pixel per destination pixel and every edge that falls between them arrives stair-stepped —
 * which is what an application's 512-pixel artwork looked like at a 64-point icon. The quality is set on the CONTEXT
 * this function made, so no caller's state is touched. */
static CGImageRef fn_iconDrawAt(CGImageRef image, size_t width, size_t height)
{
	CGContextRef ctx;
	CGImageRef out;

	if (image == NULL || width == 0 || height == 0) return NULL;
	ctx = _AGIconCreateContext(width, height);
	if (ctx == NULL) return NULL;
	CGContextSetInterpolationQuality(ctx, kCGInterpolationMedium);
	CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)width, (CGFloat)height), image);
	out = CGBitmapContextCreateImage(ctx);
	CGContextRelease(ctx);
	return out;
}

CGImageRef _AGIconRasterize(CGImageRef image, size_t width, size_t height)
{
	CGImageRef current;
	size_t cw, ch;

	if (image == NULL || width == 0 || height == 0) return NULL;
	current = CGImageRetain(image);
	cw = CGImageGetWidth(current);
	ch = CGImageGetHeight(current);
	/* **AND IT GOES DOWN IN HALVES.** One 2x2 tap across an 8:1 reduction reads four of the sixty-four source pixels
	 * behind each destination pixel, so a fine line can vanish or double; each halving with that same tap IS a box
	 * filter over the pixels it discards, and the chain of them is what a mipmap is. The last step is the one that
	 * lands on the size asked for, and it is never more than 2:1. */
	while (cw >= width * 2 && ch >= height * 2 && cw > 1 && ch > 1) {
		CGImageRef half = fn_iconDrawAt(current, cw / 2, ch / 2);

		if (half == NULL) break;
		CGImageRelease(current);
		current = half;
		cw = CGImageGetWidth(current);
		ch = CGImageGetHeight(current);
	}
	if (cw == width && ch == height) return current;
	{
		CGImageRef out = fn_iconDrawAt(current, width, height);

		CGImageRelease(current);
		return out;
	}
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

/* **AND THE SMALLEST, WHICH IS THE SIZE THE ARTWORK WAS DRAWN FOR.** A caller making an image out of an icon needs two
 * numbers: how big the thing should be ON THE SCREEN (the smallest painting — a Tabler symbol is designed on a 24-point
 * grid) and the sharpest painting to draw it from (`AGIconGetBestSize`). The theme ships 24, 48 and 96 for exactly that
 * reason, and an icon with one representation answers the same size twice. */
CGSize AGIconGetNaturalSize(IconRef icon)
{
	CGSize size = CGSizeMake(0, 0);
	if (icon == NULL) return size;
	for (size_t i = 0; i < icon->count; i++) {
		CGFloat w = (CGFloat)CGImageGetWidth(icon->reps[i]);
		if (w > 0.0 && (size.width <= 0.0 || w < size.width)) {
			size.width = w;
			size.height = (CGFloat)CGImageGetHeight(icon->reps[i]);
		}
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
