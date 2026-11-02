/*
 * AGIconLoader.c — where icons come from: an application's `.icns`, a PNG file, or a drawn generic.
 *
 * Copyright © 2026 Kyle J. Cardoza.
 * SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * THE MODEL IS APPLE'S. An ITEM's icon is its application's if it is an application bundle, and otherwise
 * the GENERIC icon for its type; a TYPE's generic is chosen by CONFORMANCE — an image, a text document, a
 * folder, an application. That is Icon Services' model, and the type comes from the sibling component
 * `libagora-launchservices`.
 *
 * THE `.icns` READER IS IN-TREE AND HANDLES THE PNG-BEARING ENTRIES, which is the modern case (ic07, ic08,
 * ic09, ic10, ic11, ic12, ic13, ic14). THE LEGACY RLE/RAW ENTRIES (is32, il32, ih32, it32 and the ic04/ic05
 * ARGB forms) ARE NOT READ YET, and that is named here rather than discovered: an `.icns` that holds only
 * those yields no icon and the caller falls back to the generic. The PNG entries are identified BY THE PNG
 * SIGNATURE rather than by a type table, so a container that puts PNG in an unexpected slot still works.
 *
 * THE GENERICS ARE DRAWN, NOT SHIPPED. Apple's generic icons are resources; Agora's default art is a THEME named by
 * `AGIconTheme.c` and rendered per machine, and this file's document, folder, application, image, text, movie, audio and
 * archive drawings are the fallback: a machine without that theme, a process that points `AGORA_ICON_THEME` somewhere
 * else, or a name the theme does not carry. They are drawn with Core Graphics at
 * a fixed 256 points — deterministic and reviewable, with no lookup needed and no binary asset of their own.
 */
#include "IconServices_internal.h"

#include <LaunchServices/LSInfo.h>
#include <LaunchServices/UTType.h>

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGColor.h>
#include <CoreGraphics/CGDataProvider.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ============================================================================================ *
 * REGISTERED OVERRIDES                                                                                *
 * ============================================================================================ */
typedef struct { CFStringRef uti; IconRef icon; } AGIconOverride;

static AGIconOverride *g_overrides = NULL;
static size_t g_overrideCount = 0, g_overrideCap = 0;

void AGIconRegisterIconForType(CFStringRef uti, IconRef icon)
{
	if (uti == NULL || icon == NULL) return;
	for (size_t i = 0; i < g_overrideCount; i++) {
		if (CFEqual(g_overrides[i].uti, uti)) { AGIconRelease(g_overrides[i].icon); g_overrides[i].icon = AGIconRetain(icon); return; }
	}
	if (g_overrideCount == g_overrideCap) {
		g_overrideCap = g_overrideCap ? g_overrideCap * 2 : 8;
		g_overrides = realloc(g_overrides, g_overrideCap * sizeof(*g_overrides));
	}
	g_overrides[g_overrideCount].uti = (CFStringRef)CFRetain(uti);
	g_overrides[g_overrideCount].icon = AGIconRetain(icon);
	g_overrideCount++;
}

void AGIconReset(void)
{
	for (size_t i = 0; i < g_overrideCount; i++) { CFRelease(g_overrides[i].uti); AGIconRelease(g_overrides[i].icon); }
	g_overrideCount = 0;
	_AGIconThemeReset();
}

static IconRef overrideForType(CFStringRef uti)
{
	if (uti == NULL) return NULL;
	for (size_t i = 0; i < g_overrideCount; i++) if (CFEqual(g_overrides[i].uti, uti)) return g_overrides[i].icon;
	for (size_t i = 0; i < g_overrideCount; i++) if (UTTypeConformsTo(uti, g_overrides[i].uti)) return g_overrides[i].icon;
	return NULL;
}

/* ============================================================================================ *
 * READING BYTES AND DECODING PNG                                                                      *
 * ============================================================================================ */
/* Read a file's raw bytes. POSIX rather than CFURLCreateData, because these files are BINARY and the
 * promise wanted is "the bytes", not "the bytes interpreted". */
static CFDataRef dataFromFile(CFURLRef url)
{
	char path[4096];
	if (!CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof path)) return NULL;
	FILE *f = fopen(path, "rb");
	if (f == NULL) return NULL;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
	long n = ftell(f);
	if (n < 0) { fclose(f); return NULL; }
	rewind(f);
	unsigned char *buf = malloc((size_t)n ? (size_t)n : 1);
	if (buf == NULL) { fclose(f); return NULL; }
	size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	CFDataRef data = CFDataCreate(NULL, buf, (CFIndex)got);
	free(buf);
	return data;
}

static bool isPNG(const unsigned char *b, size_t n)
{
	static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	return n >= 8 && memcmp(b, sig, 8) == 0;
}

static CGImageRef imageFromPNGData(CFDataRef data)
{
	if (data == NULL) return NULL;
	CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
	if (provider == NULL) return NULL;
	CGImageRef image = CGImageCreateWithPNGDataProvider(provider, NULL, false, kCGRenderingIntentDefault);
	CGDataProviderRelease(provider);
	return image;
}

/* ============================================================================================ *
 * THE .icns CONTAINER                                                                                 *
 * ============================================================================================ */
static IconRef iconFromICNSData(CFDataRef data)
{
	const unsigned char *b = CFDataGetBytePtr(data);
	size_t n = (size_t)CFDataGetLength(data);
	if (n < 8 || memcmp(b, "icns", 4) != 0) return NULL;

	IconRef icon = _AGIconCreateEmpty();
	size_t off = 8;
	while (off + 8 <= n) {
		uint32_t len = ((uint32_t)b[off + 4] << 24) | ((uint32_t)b[off + 5] << 16) |
		               ((uint32_t)b[off + 6] << 8) | (uint32_t)b[off + 7];
		if (len < 8 || off + len > n) break;
		const unsigned char *payload = b + off + 8;
		size_t plen = (size_t)len - 8;
		if (isPNG(payload, plen)) {
			CFDataRef pd = CFDataCreate(NULL, payload, (CFIndex)plen);
			CGImageRef image = imageFromPNGData(pd);
			CFRelease(pd);
			if (image != NULL) { _AGIconAddRepresentation(icon, image); CGImageRelease(image); }
		}
		off += len;
	}
	if (icon->count == 0) { AGIconRelease(icon); return NULL; }
	return icon;
}

/* ============================================================================================ *
 * AN APPLICATION'S OWN ICON                                                                           *
 * ============================================================================================ */
/* **A BUNDLE'S ICON IS THE BUNDLE'S, AND THE SEARCH FOR IT IS WHAT "ALWAYS" MEANS.** A bundle names its picture in
 * `CFBundleIconFile` and this used to stop there: a bundle whose manifest named nothing — or named a file the build did
 * not ship — answered NULL and the caller drew the theme's generic application glyph. MEASURED as "Workspace does not
 * recognise .app bundles in the trash", where the trashed copy is an OLDER BUILD with no `Contents/Resources` at all.
 *
 * So the search is four passes, cheapest first:
 *
 *   1. `CFBundleIconFile`, with `.icns` and `.png` tried when the name carries no extension.
 *   2. The CONVENTIONAL names — `CFBundleName`, `CFBundleExecutable`, `icon`, `AppIcon` — against the same resources.
 *   3. The one picture a bundle that is not following the convention still ships: the first `.icns` in
 *      `Contents/Resources`, and failing that the first `.png`.
 *   4. **The APPLICATION, BY IDENTIFIER.** A copy that has been trashed or moved is still that application: Launch
 *      Services names its installed copies, and the first one that is not this bundle and does carry an icon answers.
 *      ONE hop only — the recursive call passes `false` — so two bundles that pointed at each other cannot recurse.
 */
static IconRef iconFromFileAtPath(const char *full)
{
	CFStringRef fs = CFStringCreateWithCString(NULL, full, kCFStringEncodingUTF8);
	CFURLRef url;
	CFDataRef data;
	IconRef result = NULL;

	if (fs == NULL) return NULL;
	url = CFURLCreateWithFileSystemPath(NULL, fs, kCFURLPOSIXPathStyle, false);
	CFRelease(fs);
	if (url == NULL) return NULL;
	data = dataFromFile(url);
	CFRelease(url);
	if (data == NULL) return NULL;
	{
		const unsigned char *bytes = CFDataGetBytePtr(data);
		size_t length = (size_t)CFDataGetLength(data);

		if (length >= 8 && memcmp(bytes, "icns", 4) == 0) {
			result = iconFromICNSData(data);
		} else if (isPNG(bytes, length)) {
			CGImageRef image = imageFromPNGData(data);

			if (image != NULL) {
				result = AGIconCreateWithImage(image);
				CGImageRelease(image);
			}
		}
	}
	CFRelease(data);
	return result;
}

/* A name against `Contents/Resources`: the name as given when it carries an extension, otherwise both forms. */
static IconRef iconNamedInBundle(const char *basec, const char *name)
{
	char candidates[2][600];
	int count = 0;

	if (name == NULL || name[0] == '\0') return NULL;
	if (strchr(name, '.') != NULL) {
		snprintf(candidates[count++], sizeof candidates[0], "%s", name);
	} else {
		snprintf(candidates[count++], sizeof candidates[0], "%s.icns", name);
		snprintf(candidates[count++], sizeof candidates[0], "%s.png", name);
	}
	for (int i = 0; i < count; i++) {
		char full[4096];
		IconRef icon;

		snprintf(full, sizeof full, "%s/Contents/Resources/%s", basec, candidates[i]);
		icon = iconFromFileAtPath(full);
		if (icon != NULL) return icon;
	}
	return NULL;
}

/* **THE ONE PICTURE A BUNDLE STILL SHIPS.** `Contents/Resources` read once, `.icns` preferred over `.png` — a bundle
 * that carries one picture means it, whatever it called it. */
static IconRef iconAnyInBundle(const char *basec)
{
	char directory[4096];
	DIR *dir;
	struct dirent *entry;
	IconRef result = NULL;

	snprintf(directory, sizeof directory, "%s/Contents/Resources", basec);
	dir = opendir(directory);
	if (dir == NULL) return NULL;
	for (int pass = 0; pass < 2 && result == NULL; pass++) {
		const char *want = (pass == 0) ? ".icns" : ".png";

		rewinddir(dir);
		while ((entry = readdir(dir)) != NULL) {
			size_t length = strlen(entry->d_name);
			size_t suffix = strlen(want);
			char full[4096];

			if (length <= suffix || strcmp(entry->d_name + (length - suffix), want) != 0) {
				continue;
			}
			snprintf(full, sizeof full, "%s/%s", directory, entry->d_name);
			result = iconFromFileAtPath(full);
			if (result != NULL) break;
		}
	}
	closedir(dir);
	return result;
}

static IconRef iconForApplicationAt(CFURLRef appURL, bool allowIdentifierLookup)
{
	CFDictionaryRef info = CFBundleCopyInfoDictionaryForURL(appURL);
	CFStringRef base = CFURLCopyFileSystemPath(appURL, kCFURLPOSIXPathStyle);
	char basec[4096];
	IconRef result = NULL;

	if (base == NULL) {
		if (info != NULL) CFRelease(info);
		return NULL;
	}
	CFStringGetCString(base, basec, sizeof basec, kCFStringEncodingUTF8);
	CFRelease(base);

	if (info != NULL) {
		static const CFStringRef keys[2] = { CFSTR("CFBundleIconFile"), CFSTR("CFBundleName") };

		for (int pass = 0; pass < 4 && result == NULL; pass++) {
			CFTypeRef value = NULL;
			char name[512];

			if (pass < 2) {
				value = CFDictionaryGetValue(info, keys[pass]);
			} else if (pass == 2) {
				value = CFDictionaryGetValue(info, CFSTR("CFBundleExecutable"));
			}
			if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID()) {
				CFStringGetCString((CFStringRef)value, name, sizeof name, kCFStringEncodingUTF8);
				result = iconNamedInBundle(basec, name);
			} else if (pass == 3) {
				result = iconNamedInBundle(basec, "icon");
				if (result == NULL) result = iconNamedInBundle(basec, "AppIcon");
			}
		}
		CFRelease(info);
	}
	if (result == NULL) {
		result = iconAnyInBundle(basec);
	}
	if (result == NULL && allowIdentifierLookup) {
		CFDictionaryRef dictionary = CFBundleCopyInfoDictionaryForURL(appURL);

		if (dictionary != NULL) {
			CFTypeRef identifier = CFDictionaryGetValue(dictionary, CFSTR("CFBundleIdentifier"));

			if (identifier != NULL && CFGetTypeID(identifier) == CFStringGetTypeID()) {
				CFArrayRef urls = LSCopyApplicationURLsForBundleIdentifier((CFStringRef)identifier, NULL);

				if (urls != NULL) {
					CFIndex i, total = CFArrayGetCount(urls);

					for (i = 0; i < total && result == NULL; i++) {
						CFURLRef candidate = (CFURLRef)CFArrayGetValueAtIndex(urls, i);
						CFStringRef candidatePath = CFURLCopyFileSystemPath(candidate, kCFURLPOSIXPathStyle);

						if (candidatePath != NULL) {
							char candidatec[4096];

							CFStringGetCString(candidatePath, candidatec, sizeof candidatec,
									   kCFStringEncodingUTF8);
							if (strcmp(candidatec, basec) != 0) {
								result = iconForApplicationAt(candidate, false);
							}
							CFRelease(candidatePath);
						}
					}
					CFRelease(urls);
				}
			}
			CFRelease(dictionary);
		}
	}
	return result;
}

static IconRef iconForApplication(CFURLRef appURL)
{
	return iconForApplicationAt(appURL, true);
}

/* ============================================================================================ *
 * THE DRAWN GENERICS                                                                                  *
 * ============================================================================================ */
enum { AGCatDocument, AGCatFolder, AGCatApplication, AGCatImage, AGCatText, AGCatMovie, AGCatAudio, AGCatArchive };

static int categoryOf(CFStringRef uti)
{
	if (UTTypeConformsTo(uti, kUTTypeApplicationBundle) || UTTypeConformsTo(uti, kUTTypeApplication)) return AGCatApplication;
	if (UTTypeConformsTo(uti, kUTTypeFolder) || UTTypeConformsTo(uti, kUTTypeDirectory)) return AGCatFolder;
	if (UTTypeConformsTo(uti, kUTTypeImage)) return AGCatImage;
	if (UTTypeConformsTo(uti, kUTTypeText)) return AGCatText;
	if (UTTypeConformsTo(uti, kUTTypeMovie)) return AGCatMovie;
	if (UTTypeConformsTo(uti, kUTTypeAudio)) return AGCatAudio;
	if (UTTypeConformsTo(uti, kUTTypeArchive)) return AGCatArchive;
	return AGCatDocument;
}

static void fill(CGContextRef ctx, CGFloat r, CGFloat g, CGFloat b, CGFloat a)
{
	CGColorRef c = CGColorCreateGenericRGB(r, g, b, a);
	CGContextSetFillColorWithColor(ctx, c);
	CGColorRelease(c);
}

static CGImageRef generateGeneric(CFStringRef uti)
{
	const size_t S = 256;
	CGContextRef ctx = _AGIconCreateContext(S, S);
	if (ctx == NULL) return NULL;

	int cat = categoryOf(uti);
	CGFloat r = 0.45, g = 0.45, b = 0.50;
	switch (cat) {
		case AGCatFolder:      r = 0.30; g = 0.55; b = 0.90; break;
		case AGCatApplication: r = 0.35; g = 0.35; b = 0.40; break;
		case AGCatImage:       r = 0.20; g = 0.65; b = 0.35; break;
		case AGCatText:        r = 0.55; g = 0.55; b = 0.60; break;
		case AGCatMovie:       r = 0.70; g = 0.35; b = 0.70; break;
		case AGCatAudio:       r = 0.90; g = 0.55; b = 0.20; break;
		case AGCatArchive:     r = 0.60; g = 0.45; b = 0.20; break;
		default: break;
	}

	if (cat == AGCatFolder) {
		fill(ctx, 0.40, 0.50, 0.65, 1); CGContextFillRect(ctx, CGRectMake(24, 40, 208, 176));
		fill(ctx, 0.72, 0.84, 0.98, 1); CGContextFillRect(ctx, CGRectMake(30, 48, 196, 160));
		fill(ctx, 0.40, 0.50, 0.65, 1); CGContextFillRect(ctx, CGRectMake(24, 196, 96, 28));
		fill(ctx, 0.72, 0.84, 0.98, 1); CGContextFillRect(ctx, CGRectMake(30, 200, 84, 20));
	} else {
		fill(ctx, 0.55, 0.55, 0.58, 1); CGContextFillRect(ctx, CGRectMake(48, 28, 160, 200));
		fill(ctx, 0.97, 0.97, 0.98, 1); CGContextFillRect(ctx, CGRectMake(54, 34, 148, 188));
		fill(ctx, r, g, b, 1);          CGContextFillRect(ctx, CGRectMake(80, 96, 96, 64));
	}
	CGImageRef out = CGBitmapContextCreateImage(ctx);
	CGContextRelease(ctx);
	return out;
}

/* ============================================================================================ *
 * THE DOORS' HELPERS                                                                                  *
 * ============================================================================================ */
IconRef AGIconCreateWithType(CFStringRef uti)
{
	IconRef over = overrideForType(uti);
	if (over != NULL) return AGIconRetain(over);

	/* THE THEME FIRST, THE DRAWING SECOND. A theme's names are the freedesktop ones; a type it does
	 * not carry falls through to the generic that this component draws. */
	CFStringRef type = uti != NULL ? uti : kUTTypeData;
	IconRef themed = _AGIconCreateWithThemeType(type);
	if (themed != NULL) return themed;

	CGImageRef image = generateGeneric(type);
	if (image == NULL) return NULL;
	IconRef icon = AGIconCreateWithImage(image);
	CGImageRelease(image);
	return icon;
}

/* The theme's place icon for a directory: the well-known folders get their own art and everything else is a
 * folder. The names are the freedesktop ones and the comparison is the path the URL already resolved to. */
static const char *placeNameForPath(const char *path)
{
	const char *home = getenv("HOME");
	if (path != NULL && home != NULL && home[0] != '\0') {
		char candidate[4096];
		if (strcmp(path, home) == 0) return "user-home";
		snprintf(candidate, sizeof candidate, "%s/Desktop", home);
		if (strcmp(path, candidate) == 0) return "user-desktop";
		snprintf(candidate, sizeof candidate, "%s/Downloads", home);
		if (strcmp(path, candidate) == 0) return "folder-download";
		snprintf(candidate, sizeof candidate, "%s/Documents", home);
		if (strcmp(path, candidate) == 0) return "folder-documents";
		snprintf(candidate, sizeof candidate, "%s/Pictures", home);
		if (strcmp(path, candidate) == 0) return "folder-pictures";
		snprintf(candidate, sizeof candidate, "%s/Music", home);
		if (strcmp(path, candidate) == 0) return "folder-music";
		snprintf(candidate, sizeof candidate, "%s/Videos", home);
		if (strcmp(path, candidate) == 0) return "folder-video";
		snprintf(candidate, sizeof candidate, "%s/.Trash", home);
		if (strcmp(path, candidate) == 0) return "user-trash";
	}
	return "folder";
}

IconRef _AGIconCreateGeneric(CFStringRef uti) { return AGIconCreateWithType(uti); }

IconRef _AGIconCreateForURL(CFURLRef url)
{
	if (url == NULL) return NULL;

	bool isDir = false;
	char path[4096];
	bool hasPath = CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof path);
	if (hasPath) {
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) isDir = true;
	}
	CFStringRef ext = CFURLCopyPathExtension(url);
	bool isApp = isDir && ext != NULL &&
	             CFStringCompare(ext, CFSTR("app"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
	if (ext != NULL) CFRelease(ext);

	if (isApp) {
		IconRef own = iconForApplication(url);
		if (own != NULL) return own;
		return AGIconCreateWithType(kUTTypeApplicationBundle);
	}
	if (isDir) {
		IconRef placed = _AGIconCreateWithThemeName(placeNameForPath(hasPath ? path : NULL));
		if (placed != NULL) return placed;
		return AGIconCreateWithType(kUTTypeFolder);
	}

	CFStringRef fext = CFURLCopyPathExtension(url);
	CFStringRef uti = NULL;
	if (fext != NULL) { uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, fext, NULL); CFRelease(fext); }
	IconRef icon = AGIconCreateWithType(uti != NULL ? uti : kUTTypeData);
	if (uti != NULL) CFRelease(uti);
	return icon;
}

IconRef AGIconCreateWithURL(CFURLRef url) { return _AGIconCreateForURL(url); }
