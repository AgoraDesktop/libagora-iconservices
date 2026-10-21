/*
 * AGIconTheme.c — the vendored default icon theme: find an icon by name, at the sizes it ships.
 *
 * Copyright © 2026 Kyle J. Cardoza. MIT licensed — see LICENSE.
 * SPDX-License-Identifier: MIT
 *
 * THE THEME IS FROST, AND IT IS DATA RATHER THAN A DRAWING. Apple's generic icons are resources; this tree
 * used to draw its own because it shipped no art. Agora now vendors Frost (MIT, `themes/Frost`), whose PNGs
 * are the interim default: they are looked up here, and the drawn generics in `AGIconLoader.c` remain the
 * fallback for a checkout without the theme, a name Frost does not carry, or a process that points
 * `AGORA_ICON_THEME` somewhere empty.
 *
 * WHERE A THEME IS FOUND. `AGORA_ICON_THEME` names a directory, which is what a test or a caller that wants
 * another theme sets; otherwise `AGORA_ICON_DEFAULT_THEME`, which this component's build points at its
 * installed `share/icons/Frost`. A path that is not there resolves no icon, so the drawing takes over.
 *
 * A NAME IS THE FREEDESKTOP ICON NAME. A type's name comes from its preferred MIME tag with '/' turned into
 * '-': `image/png` is `image-png`, `application/vnd.oasis.opendocument.text` is its MIME with the same
 * substitution. A type with no MIME tag falls back to the generic for its category — `text-x-generic`,
 * `folder`, `application-x-executable` — and an item the theme knows nothing about gets the last of those.
 *
 * THE SIZES ARE THE THEME'S OWN, and both scales are read: `48x48/...` and `48x48@2x/...` each become
 * representations, so a caller asking for 64 pixels gets the 64-pixel art and one asking for 512 gets the
 * 256 @2x art rather than a scaled-up 256. The PNG is decoded by Core Graphics, exactly as the `.icns`
 * reader does — this file opens no decoder of its own.
 *
 * THE CACHE IS PER PROCESS AND KEYED BY NAME, because a file window asks for the same few types once per
 * row. A miss is cached too: a dynamic UTI that no theme name matches should not cost a directory walk per
 * file. `AGIconReset` clears it, and a change to `AGORA_ICON_THEME` clears it as well.
 */
#include "IconServices_internal.h"

#include <LaunchServices/UTType.h>

#include <CoreGraphics/CGDataProvider.h>
#include <CoreGraphics/CGImage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AGORA_ICON_DEFAULT_THEME
#define AGORA_ICON_DEFAULT_THEME ""
#endif

#define AG_THEME_PATH_MAX 4096
#define AG_THEME_NAME_MAX 512
#define AG_THEME_CACHE_MAX 1024

/* Every size Frost ships, at both scales. A size the theme lacks is simply a miss. */
static const int kThemeSizes[] = { 16, 22, 24, 32, 48, 64, 96, 128, 256 };

/* The contexts a name can live in. `places` carries folders, `mimetypes` carries file types, and `apps`
 * carries application art. A name is looked for in all three because the mapping is a convention and not a
 * promise: a theme may put `folder` in any of them. */
static const char *const kThemeContexts[] = { "places", "mimetypes", "apps" };

typedef struct {
	char *name;
	IconRef icon;		/* NULL is a cached miss */
} AGThemeCacheEntry;

static AGThemeCacheEntry *g_themeCache = NULL;
static size_t g_themeCacheCount = 0, g_themeCacheCap = 0;
static char g_themeRoot[AG_THEME_PATH_MAX];
static bool g_themeRootKnown = false;

static void themeCacheClear(void)
{
	for (size_t i = 0; i < g_themeCacheCount; i++) {
		free(g_themeCache[i].name);
		AGIconRelease(g_themeCache[i].icon);
	}
	g_themeCacheCount = 0;
}

void _AGIconThemeReset(void)
{
	themeCacheClear();
}

/* The theme directory this process is using, or NULL. A change of `AGORA_ICON_THEME` between calls is
 * noticed here and drops the cache, which is what makes a test that points the process at another theme
 * (or at none) honest. */
static const char *themeRoot(void)
{
	const char *env = getenv("AGORA_ICON_THEME");
	const char *root = (env != NULL && env[0] != '\0') ? env : AGORA_ICON_DEFAULT_THEME;
	if (root == NULL) root = "";

	if (!g_themeRootKnown || strcmp(g_themeRoot, root) != 0) {
		themeCacheClear();
		snprintf(g_themeRoot, sizeof g_themeRoot, "%s", root);
		g_themeRootKnown = true;
	}
	return g_themeRoot[0] != '\0' ? g_themeRoot : NULL;
}

static bool validName(const char *name)
{
	if (name == NULL || name[0] == '\0') return false;
	if (strlen(name) >= AG_THEME_NAME_MAX) return false;
	if (strstr(name, "..") != NULL) return false;
	return strchr(name, '/') == NULL;
}

/* A PNG at `path`, or NULL. `access` first because the drawing layer's file provider is noisy about a
 * file that is simply not there, and a name missing at one size is the ordinary case rather than an error. */
static CGImageRef imageAtPath(const char *path)
{
	if (access(path, R_OK) != 0) return NULL;
	CGDataProviderRef provider = CGDataProviderCreateWithFilename(path);
	if (provider == NULL) return NULL;
	CGImageRef image = CGImageCreateWithPNGDataProvider(provider, NULL, false, kCGRenderingIntentDefault);
	CGDataProviderRelease(provider);
	return image;
}

static IconRef loadThemeIconUncached(const char *root, const char *name)
{
	IconRef icon = _AGIconCreateEmpty();
	if (icon == NULL) return NULL;

	for (size_t s = 0; s < sizeof kThemeSizes / sizeof kThemeSizes[0]; s++) {
		int size = kThemeSizes[s];
		for (int scale = 1; scale <= 2; scale++) {
			for (size_t c = 0; c < sizeof kThemeContexts / sizeof kThemeContexts[0]; c++) {
				char path[AG_THEME_PATH_MAX];
				snprintf(path, sizeof path, "%s/%dx%d%s/%s/%s.png", root, size, size,
				         scale == 2 ? "@2x" : "", kThemeContexts[c], name);
				CGImageRef image = imageAtPath(path);
				if (image != NULL) {
					_AGIconAddRepresentation(icon, image);
					CGImageRelease(image);
					break;	/* this size and scale is found; the next one may be elsewhere */
				}
			}
		}
	}
	if (icon->count == 0) { AGIconRelease(icon); return NULL; }
	return icon;
}

/* The theme's icon for `name`, +1, or NULL. The cache holds the only reference; a caller gets its own. */
static IconRef themeIconNamed(const char *name)
{
	if (!validName(name)) return NULL;
	const char *root = themeRoot();
	if (root == NULL) return NULL;

	for (size_t i = 0; i < g_themeCacheCount; i++) {
		if (strcmp(g_themeCache[i].name, name) == 0)
			return g_themeCache[i].icon != NULL ? AGIconRetain(g_themeCache[i].icon) : NULL;
	}

	IconRef icon = loadThemeIconUncached(root, name);
	if (g_themeCacheCount == g_themeCacheCap) {
		size_t cap = g_themeCacheCap ? g_themeCacheCap * 2 : 64;
		if (cap > AG_THEME_CACHE_MAX) {
			themeCacheClear();
			cap = 64;
		}
		AGThemeCacheEntry *grown = realloc(g_themeCache, cap * sizeof *grown);
		if (grown == NULL) return icon;	/* uncached, but still the right answer */
		g_themeCache = grown;
		g_themeCacheCap = cap;
	}
	g_themeCache[g_themeCacheCount].name = strdup(name);
	if (g_themeCache[g_themeCacheCount].name == NULL) return icon;
	g_themeCache[g_themeCacheCount].icon = icon;
	g_themeCacheCount++;
	return icon != NULL ? AGIconRetain(icon) : NULL;
}

/* One MIME tag as a freedesktop icon name: lower-case, '/' becomes '-', parameters are dropped, and a
 * character the name syntax cannot carry is dropped rather than allowed to escape the directory. */
static bool iconNameFromMIME(CFStringRef mime, char *out, size_t cap)
{
	char buffer[AG_THEME_NAME_MAX];
	if (!CFStringGetCString(mime, buffer, sizeof buffer, kCFStringEncodingUTF8)) return false;

	size_t o = 0;
	for (const char *p = buffer; *p != '\0' && o + 1 < cap; p++) {
		unsigned char c = (unsigned char)*p;
		if (c == ';') break;
		if (c == '/') c = '-';
		else if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		    c == '-' || c == '+' || c == '.' || c == '_') {
			out[o++] = (char)c;
		}
	}
	out[o] = '\0';
	return o > 0;
}

/* The generic for a type's category, in Frost's names. */
static const char *genericNameForType(CFStringRef uti)
{
	if (uti != NULL) {
		if (UTTypeConformsTo(uti, kUTTypeApplicationBundle) ||
		    UTTypeConformsTo(uti, kUTTypeApplication) ||
		    UTTypeConformsTo(uti, kUTTypeExecutable) ||
		    UTTypeConformsTo(uti, kUTTypeUnixExecutable)) return "application-x-executable";
		if (UTTypeConformsTo(uti, kUTTypeFolder) || UTTypeConformsTo(uti, kUTTypeDirectory))
			return "folder";
		if (UTTypeConformsTo(uti, kUTTypeImage)) return "image-x-generic";
		if (UTTypeConformsTo(uti, kUTTypeText) || UTTypeConformsTo(uti, kUTTypeSourceCode))
			return "text-x-generic";
		if (UTTypeConformsTo(uti, kUTTypeMovie)) return "video-x-generic";
		if (UTTypeConformsTo(uti, kUTTypeAudio)) return "audio-x-generic";
		if (UTTypeConformsTo(uti, kUTTypeArchive)) return "application-x-archive";
	}
	return "application-x-generic";
}

IconRef _AGIconCreateWithThemeType(CFStringRef uti)
{
	if (uti != NULL) {
		CFStringRef mime = UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType);
		if (mime != NULL) {
			char name[AG_THEME_NAME_MAX];
			bool named = iconNameFromMIME(mime, name, sizeof name);
			CFRelease(mime);
			if (named) {
				IconRef icon = themeIconNamed(name);
				if (icon != NULL) return icon;
			}
		}
	}
	return themeIconNamed(genericNameForType(uti));
}

IconRef _AGIconCreateWithThemeName(const char *name)
{
	return themeIconNamed(name);
}
