/*
 * AGIconTheme.c — the vendored default icon theme: find an icon by name, at the sizes it ships.
 *
 * Copyright © 2026 Kyle J. Cardoza.
 * SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * A THEME IS DATA RATHER THAN A DRAWING, AND THE DEFAULT ONE IS VENDORED. Apple's generic icons are resources; this tree
 * used to draw its own because it shipped no art. The default now names `share/icons/Flat-Remix-Accent` — Flat-Remix with
 * its folders in the accent, rendered by `demo/flat-remix-accent.sh` and committed as its PNGs (GPL-3.0, like this
 * component). Its art is looked up here, and the drawn generics in `AGIconLoader.c` remain the fallback for a name the
 * theme does not carry or a process that points `AGORA_ICON_THEME` somewhere empty.
 *
 * WHERE A THEME IS FOUND. `AGORA_ICON_THEME` names a directory, which is what a test or a caller that wants
 * another theme sets; otherwise `AGORA_ICON_DEFAULT_THEME`, which this component's build points at
 * `share/icons/Flat-Remix-Accent`. A path that is not there resolves no icon, so the drawing takes over.
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
#include <strings.h>	/* `strcasecmp`: a theme prefix is matched without case */
#include <unistd.h>

#ifndef AGORA_ICON_DEFAULT_THEME
#define AGORA_ICON_DEFAULT_THEME ""
#endif

#define AG_THEME_PATH_MAX 4096
#define AG_THEME_NAME_MAX 512
#define AG_THEME_CACHE_MAX 1024

/* Every size a theme may ship, at both scales. A size the theme lacks is simply a miss. */
static const int kThemeSizes[] = { 16, 22, 24, 32, 48, 64, 96, 128, 256 };

/* The contexts a name can live in. `places` carries folders, `mimetypes` carries file types, `apps` carries application
 * art, and `actions` carries the UI's own symbols — the toolbar verbs and switches, which is where a vendored symbol set
 * belongs. A name is looked for in ALL of them because the mapping is a convention and not a promise: a theme may put
 * `folder` in any of them, and Tabler's `trash` sits in `actions` beside a desktop theme's `places`. */
static const char *const kThemeContexts[] = { "actions", "places", "mimetypes", "apps" };

typedef struct {
	char *root;		/* WHICH theme answered: the same name is a different picture in each */
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
		free(g_themeCache[i].root);
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

/* **AND A THEME'S NEIGHBOURS ARE ASKED BEFORE THE ANSWER IS NO.** `AGORA_ICON_THEME` names the theme IN CHARGE, and the
 * art a desktop provides is not all in one directory: the theme in charge is places, file types and application art, and
 * Tabler is the UI's symbols — installed beside it under `share/icons`. So a name the primary theme does not carry is
 * asked of its sibling `Tabler`, which is the whole of "Tabler beside the theme in charge": a session that points
 * `AGORA_ICON_THEME` at a theme in a checkout finds `hammer` in `themes/Tabler` beside it, and an installed one finds it
 * in `share/icons/Tabler`. The reverse needs no rule — a theme named `Tabler` has no sibling to ask. */
static bool siblingThemeRoot(const char *root, char *out, size_t cap)
{
	const char *slash = strrchr(root, '/');
	const char *base = (slash != NULL) ? slash + 1 : root;
	int written;

	if (strcmp(base, "Tabler") == 0) {
		return false;
	}
	if (slash != NULL) {
		written = snprintf(out, cap, "%.*s/Tabler", (int)(slash - root), root);
	} else {
		written = snprintf(out, cap, "Tabler");
	}
	return written > 0 && (size_t)written < cap;
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

/* **A NAME IS ASKED OF A THEME, AND THE CACHE REMEMBERS WHICH ONE ANSWERED.** The same string is a different picture in
 * each theme — `folder-open` is a desktop theme's filled, coloured place folder and Tabler's outline symbol — so the cache key is
 * the (theme directory, name) pair and not the name alone. */
static IconRef themeIconFromRoot(const char *root, const char *name)
{
	if (!validName(name)) return NULL;

	for (size_t i = 0; i < g_themeCacheCount; i++) {
		if (strcmp(g_themeCache[i].root, root) == 0 && strcmp(g_themeCache[i].name, name) == 0)
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
	g_themeCache[g_themeCacheCount].root = strdup(root);
	g_themeCache[g_themeCacheCount].name = strdup(name);
	if (g_themeCache[g_themeCacheCount].root == NULL || g_themeCache[g_themeCacheCount].name == NULL) {
		free(g_themeCache[g_themeCacheCount].root);
		free(g_themeCache[g_themeCacheCount].name);
		return icon;
	}
	g_themeCache[g_themeCacheCount].icon = icon;
	g_themeCacheCount++;
	return icon != NULL ? AGIconRetain(icon) : NULL;
}

/* **A THEME BY NAME, NEXT TO THE THEME IN CHARGE.** `<prefix>:<name>` says WHICH theme to ask, and the prefix is matched
 * against the directories beside the one `AGORA_ICON_THEME` names — `share/icons/Tabler`, `share/icons/Flat-Remix-Accent` — because
 * that is where a desktop's themes live. Matched without case, since a person typing `tabler:` means the same directory
 * as `Tabler`. Answers false when no directory has that name, in which case the caller treats the whole string as an
 * ordinary name (and misses, which is the same silence a name no theme carries gets). */
static bool themeNamedBeside(const char *root, const char *themeName, char *out, size_t cap)
{
	const char *slash = strrchr(root, '/');
	char parent[AG_THEME_PATH_MAX];
	DIR *dir;
	struct dirent *entry;
	bool found = false;

	if (themeName == NULL || themeName[0] == '\0') return false;
	if (slash != NULL) {
		int written = snprintf(parent, sizeof parent, "%.*s", (int)(slash - root), root);
		if (written <= 0 || (size_t)written >= sizeof parent) return false;
	} else {
		snprintf(parent, sizeof parent, ".");
	}
	dir = opendir(parent);
	if (dir == NULL) return false;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		if (strcasecmp(entry->d_name, themeName) != 0) continue;
		if (snprintf(out, cap, "%s/%s", parent, entry->d_name) > 0) found = true;
		break;
	}
	closedir(dir);
	return found && strlen(out) > 0;
}

/* The theme in charge's icon for a name, and then its sibling's — the order a PLACE and a TYPE want, because a folder's
 * art is the theme's own, and the order an UNPREFIXED name gets. */
static IconRef themeIconNamed(const char *name)
{
	const char *root = themeRoot();
	char sibling[AG_THEME_PATH_MAX];
	IconRef icon;

	if (root == NULL) return NULL;
	icon = themeIconFromRoot(root, name);
	if (icon == NULL && siblingThemeRoot(root, sibling, sizeof sibling)) {
		icon = themeIconFromRoot(sibling, name);
	}
	return icon;
}

/* **`toolbar:` IS AN ALIAS FOR THE THEME THAT HOLDS THE UI'S SYMBOLS**, so a band asks for
 * `toolbar:folder-open` and gets the outline without naming `Tabler`: the vendored set can be replaced — another theme
 * that carries the symbols, or a person's own — without every caller changing, and a caller does not have to know which
 * directory currently answers. It is resolved BEFORE the directory match, so `toolbar:` is never read as a directory
 * name. */
static const char *fn_theme_alias(const char *prefix)
{
	if (strcasecmp(prefix, "toolbar") == 0) {
		return "Tabler";
	}
	return prefix;
}

/* **AND THE PREFIXED FORM: `tabler:folder-open`, `frost:folder-open`, `toolbar:file-plus`.** The prefix names the theme
 * and the rest is the name in it, so a caller that means a SYMBOL says so instead of relying on an order — MEASURED,
 * Forge's Open row drew a solid place folder in a template band because `folder-open` exists in both themes and
 * the unprefixed order answered the theme in charge first. A prefix that names no theme is not a guess: nothing answers,
 * which is the same silence a name no theme carries gets. AN UNADORNED NAME ASKS THE THEME IN CHARGE — the default, unless a
 * person named another through `AGORA_ICON_THEME` — and then its sibling, which is the order a place and a type want. */
static IconRef themeIconSpecified(const char *spec)
{
	const char *root = themeRoot();
	const char *colon;
	char themeName[AG_THEME_PATH_MAX];
	char directory[AG_THEME_PATH_MAX];

	if (root == NULL) return NULL;
	colon = strchr(spec, ':');
	if (colon != NULL && colon != spec && strchr(spec, '/') == NULL) {
		size_t length = (size_t)(colon - spec);

		if (length < sizeof themeName) {
			memcpy(themeName, spec, length);
			themeName[length] = '\0';
			if (themeNamedBeside(root, fn_theme_alias(themeName), directory, sizeof directory)) {
				return themeIconFromRoot(directory, colon + 1);
			}
		}
	}
	return themeIconNamed(spec);
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

/* The generic for a type's category, in the freedesktop names. */
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

/* **THE PUBLIC NAME DOOR.** A symbol's name is a name, not a type: `AGIconCreateWithType` would have to be given a UTI
 * for "hammer" that no uniform type database has. This is the same lookup the places and types go through, exposed. */
IconRef AGIconCreateWithName(CFStringRef name)
{
	char buffer[AG_THEME_NAME_MAX];
	if (name == NULL) return NULL;
	if (!CFStringGetCString(name, buffer, sizeof buffer, kCFStringEncodingUTF8)) return NULL;
	/* `<theme>:<name>` names the theme as well as the picture — `tabler:folder-open` is the symbol theme's outline where
	 * `folder-open` alone is the theme in charge's filled place folder — and a name with no prefix is looked up the way
	 * a place and a type are: the theme in charge, then its sibling. */
	return themeIconSpecified(buffer);
}
