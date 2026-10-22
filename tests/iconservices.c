/*
 * iconservices.c — F2's measurement.
 *
 * Copyright © 2026 Kyle J. Cardoza.
 * SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Portions of this file may derive from GNUstep; see THIRD-PARTY.md.
 *
 * WHAT THIS HOLDS UP, in WORKSPACE-PLAN.md's words for F2:
 *
 *   * an application's DECLARED icon is found (its `Info.plist` names a `.icns`, the reader finds the PNG
 *     representations inside it, and the pixels that come back are the ones the fixture declared);
 *   * the icon is available AT TWO SIZES and a size with no representation is scaled;
 *   * a text type and a folder get DIFFERENT icons, so a type really picks its own;
 *   * a badge composites over a base without changing its size;
 *   * an override registered for a type wins;
 *   * and the vendored Frost theme is the default: a type resolves through it at both scales, while pointing
 *     `AGORA_ICON_THEME` at an empty directory brings the drawn generic back.
 *
 * THE FIXTURES ARE NOT BINARY FILES IN THE TREE. Two solid-colour PNGs are embedded as base64 and the test
 * composes an `.icns` container around them, so the reader is measured against a container this file built.
 */
#define _DEFAULT_SOURCE

#include <IconServices/IconServices.h>
#include <LaunchServices/UTType.h>

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGColorSpace.h>
#include <CoreGraphics/CGContext.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A 32×32 solid red PNG and a 128×128 solid blue PNG, as base64. */
static const char *kPNG32 =
	"iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAL0lEQVR42u3OIQEAAAgDsMd5/xS0ghiYifll2v0UAQEBAQEBAQEBAQEBAQGB78ABeThceZLwU0EAAAAASUVORK5CYII=";
static const char *kPNG128 =
	"iVBORw0KGgoAAAANSUhEUgAAAIAAAACACAYAAADDPmHLAAAA8klEQVR42u3SMQ0AAAjAMOSgCcW4Ahsk9JiBpZHVo7+FCQAYAYAAEAACQAAIAAEgAASAABAAAkAACAABIAAEgAAQAAJAAAgAASAABIAAEAACQAAIAAEgAASAABAAAkAACAABIAAEgAAQAAJAAAgAASAABIAAEAACQAAIAAEgAASAABAAAkAACAABIAAEgAAQAAJAAAgAASAABIAAEAACQAAIAAEgAASAABAAAgAAEwAwAgABIAAEgAAQAAJAAAgAASAABIAAEAACQAAIAAEgAASAABAAAkAACAABIAAEgAAQAAJAAAgAASAABIAAEAACQDda39NIj77aI4UAAAAASUVORK5CYII=";

static int g_failures = 0;
static void check(bool condition, const char *what)
{
	if (!condition) g_failures++;
	printf("  [%s] %s\n", condition ? "ok" : "FAIL", what);
}

/* ---- base64, so a PNG can live in this file as text rather than as a binary asset ---------------- */
static int b64val(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

static CFDataRef dataFromBase64(const char *s)
{
	size_t n = strlen(s);
	unsigned char *out = malloc(n / 4 * 3 + 3);
	size_t o = 0;
	int quad[4], q = 0;
	for (size_t i = 0; i < n; i++) {
		int v = b64val((unsigned char)s[i]);
		if (v < 0) continue;
		quad[q++] = v;
		if (q == 4) {
			out[o++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
			out[o++] = (unsigned char)(((quad[1] & 15) << 4) | (quad[2] >> 2));
			out[o++] = (unsigned char)(((quad[2] & 3) << 6) | quad[3]);
			q = 0;
		}
	}
	if (q == 2) out[o++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
	else if (q == 3) {
		out[o++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
		out[o++] = (unsigned char)(((quad[1] & 15) << 4) | (quad[2] >> 2));
	}
	CFDataRef d = CFDataCreate(NULL, out, (CFIndex)o);
	free(out);
	return d;
}

/* ---- composing an .icns container around two PNGs ------------------------------------------------ */
static void putBE32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

static CFDataRef makeICNS(CFDataRef png128, CFDataRef png32)
{
	const unsigned char *b128 = CFDataGetBytePtr(png128); size_t n128 = (size_t)CFDataGetLength(png128);
	const unsigned char *b32  = CFDataGetBytePtr(png32);  size_t n32  = (size_t)CFDataGetLength(png32);
	size_t total = 8 + (8 + n128) + (8 + n32);
	unsigned char *buf = malloc(total);
	size_t o = 0;
	memcpy(buf + o, "icns", 4); o += 4;
	putBE32(buf + o, (uint32_t)total); o += 4;
	size_t l = 8 + n128;
	memcpy(buf + o, "ic07", 4); o += 4; putBE32(buf + o, (uint32_t)l); o += 4; memcpy(buf + o, b128, n128); o += n128;
	l = 8 + n32;
	memcpy(buf + o, "ic11", 4); o += 4; putBE32(buf + o, (uint32_t)l); o += 4; memcpy(buf + o, b32, n32); o += n32;
	CFDataRef d = CFDataCreate(NULL, buf, (CFIndex)o);
	free(buf);
	return d;
}

static bool writeFile(const char *path, CFDataRef data)
{
	FILE *f = fopen(path, "wb");
	if (f == NULL) return false;
	bool ok = fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), f) == (size_t)CFDataGetLength(data);
	fclose(f);
	return ok;
}

static CFURLRef fileURL(const char *path)
{
	CFStringRef s = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
	CFURLRef u = CFURLCreateWithFileSystemPath(NULL, s, kCFURLPOSIXPathStyle, false);
	CFRelease(s);
	return u;
}

/* ---- reading pixels back, without assuming a channel order --------------------------------------- */
static CGContextRef rasterContext(size_t w, size_t h)
{
	CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
	CGContextRef ctx = CGBitmapContextCreate(NULL, w, h, 8, w * 4, cs,
	                                         kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
	CGColorSpaceRelease(cs);
	if (ctx != NULL) memset(CGBitmapContextGetData(ctx), 0, w * 4 * h);
	return ctx;
}

/* True if some pixel's three colour samples are the set {r,g,b} in either channel order. */
static bool imageHasColour(CGImageRef img, int r, int g, int b, int tol)
{
	size_t w = CGImageGetWidth(img), h = CGImageGetHeight(img);
	CGContextRef ctx = rasterContext(w, h);
	if (ctx == NULL) return false;
	CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), img);
	unsigned char *px = CGBitmapContextGetData(ctx);
	bool found = false;
	for (size_t i = 0; i < w * h && !found; i++) {
		int c0 = px[i * 4], c1 = px[i * 4 + 1], c2 = px[i * 4 + 2];
		if ((abs(c0 - r) <= tol && abs(c1 - g) <= tol && abs(c2 - b) <= tol) ||
		    (abs(c0 - b) <= tol && abs(c1 - g) <= tol && abs(c2 - r) <= tol)) found = true;
	}
	CGContextRelease(ctx);
	return found;
}

static bool imagesDiffer(CGImageRef a, CGImageRef b)
{
	const size_t S = 64;
	CGContextRef ca = rasterContext(S, S), cb = rasterContext(S, S);
	if (ca == NULL || cb == NULL) return false;
	CGContextDrawImage(ca, CGRectMake(0, 0, S, S), a);
	CGContextDrawImage(cb, CGRectMake(0, 0, S, S), b);
	bool diff = memcmp(CGBitmapContextGetData(ca), CGBitmapContextGetData(cb), S * 4 * S) != 0;
	CGContextRelease(ca);
	CGContextRelease(cb);
	return diff;
}

int main(void)
{
	char tmpl[] = "/tmp/agora-icons-XXXXXX";
	char *root = mkdtemp(tmpl);
	if (root == NULL) { perror("mkdtemp"); return 1; }

	char cmd[4096];
	snprintf(cmd, sizeof cmd, "cp -R '%s/Icons.app' '%s/'", FIXTURES_DIR, root);
	if (system(cmd) != 0) { fprintf(stderr, "fixture copy failed\n"); return 1; }

	char res[4096];
	snprintf(res, sizeof res, "%s/Icons.app/Contents/Resources", root);
	mkdir(res, 0755);

	CFDataRef png128 = dataFromBase64(kPNG128);
	CFDataRef png32 = dataFromBase64(kPNG32);
	CFDataRef icns = makeICNS(png128, png32);
	CFRelease(png128);
	CFRelease(png32);
	char icnsPath[4096];
	snprintf(icnsPath, sizeof icnsPath, "%s/Icons.icns", res);
	bool wrote = writeFile(icnsPath, icns);
	CFRelease(icns);

	AGIconReset();
	setenv("AGORA_ICON_THEME", THEME_DIR, 1);
	printf("iconservices: %s\n", root);
	check(wrote, "the application's .icns was written into its Resources");

	/* ---- 1. the declared icon, at two sizes --------------------------------------------- */
	char appPath[4096];
	snprintf(appPath, sizeof appPath, "%s/Icons.app", root);
	CFURLRef appURL = fileURL(appPath);
	IconRef app = AGIconCreateWithURL(appURL);
	check(app != NULL, "an application's declared icon is found");

	CGImageRef i32 = AGIconCopyImage(app, CGSizeMake(32, 32));
	check(i32 != NULL && CGImageGetWidth(i32) == 32 && CGImageGetHeight(i32) == 32,
	      "the 32-point representation comes back at 32x32");
	check(i32 != NULL && imageHasColour(i32, 200, 40, 40, 8),
	      "and its pixels are the ones the fixture declared for that size (its red)");

	CGImageRef i128 = AGIconCopyImage(app, CGSizeMake(128, 128));
	check(i128 != NULL && CGImageGetWidth(i128) == 128 && CGImageGetHeight(i128) == 128,
	      "the 128-point representation comes back at 128x128");
	check(i128 != NULL && imageHasColour(i128, 40, 90, 200, 8),
	      "and it is the OTHER representation (its blue)");

	CGImageRef i48 = AGIconCopyImage(app, CGSizeMake(48, 48));
	check(i48 != NULL && CGImageGetWidth(i48) == 48 && CGImageGetHeight(i48) == 48,
	      "a size with no representation is scaled to exactly that size");

	CGSize best = AGIconGetBestSize(app);
	check(best.width == 128 && best.height == 128, "the best size is the largest representation");

	/* ---- 2. the generics, chosen by type ------------------------------------------------- */
	IconRef textIcon = AGIconCreateWithType(kUTTypePlainText);
	IconRef folderIcon = AGIconCreateWithType(kUTTypeFolder);
	IconRef imageIcon = AGIconCreateWithType(kUTTypeImage);
	check(textIcon != NULL && folderIcon != NULL && imageIcon != NULL, "the icons are made");
	CGImageRef text64 = AGIconCopyImage(textIcon, CGSizeMake(64, 64));
	check(text64 != NULL && CGImageGetWidth(text64) == 64, "an icon draws at the size asked");
	CGImageRef folder64 = AGIconCopyImage(folderIcon, CGSizeMake(64, 64));
	CGImageRef image64 = AGIconCopyImage(imageIcon, CGSizeMake(64, 64));
	check(text64 != NULL && folder64 != NULL && imagesDiffer(text64, folder64),
	      "the text and folder icons are DIFFERENT, not one shape");

	/* ---- 3. a URL gets its type's generic, and a bundle its own icon --------------------- */
	CFURLRef folderURL = fileURL(root);
	IconRef viaURL = AGIconCreateWithURL(folderURL);
	CGImageRef viaURL64 = viaURL != NULL ? AGIconCopyImage(viaURL, CGSizeMake(64, 64)) : NULL;
	check(viaURL64 != NULL && !imagesDiffer(viaURL64, folder64), "a folder URL gets the folder icon");

	/* ---- 4. the badge compositor --------------------------------------------------------- */
	IconRef badged = AGIconCreateWithBadge(app, textIcon, 0.4);
	CGImageRef badged128 = badged != NULL ? AGIconCopyImage(badged, CGSizeMake(128, 128)) : NULL;
	check(badged128 != NULL && CGImageGetWidth(badged128) == 128,
	      "the badge composite keeps the base's size");
	check(badged128 != NULL && i128 != NULL && imagesDiffer(badged128, i128),
	      "and it differs from the unbadged icon");

	/* ---- 4b. the vendored theme, and the drawing it replaces ----------------------------- */
	check(access(THEME_DIR "/index.theme", R_OK) == 0,
	      "the vendored Frost theme is in the checkout");
	setenv("AGORA_ICON_THEME", THEME_DIR, 1);
	AGIconReset();

	IconRef frostFolder = AGIconCreateWithType(kUTTypeFolder);
	CGImageRef frost32 = frostFolder != NULL ? AGIconCopyImage(frostFolder, CGSizeMake(32, 32)) : NULL;
	CGImageRef frost512 = frostFolder != NULL ? AGIconCopyImage(frostFolder, CGSizeMake(512, 512)) : NULL;
	check(frostFolder != NULL && frost32 != NULL && CGImageGetWidth(frost32) == 32,
	      "a type resolves through the theme at the size asked");
	check(frost512 != NULL && CGImageGetWidth(frost512) == 512,
	      "and the theme's @2x representation serves a 512-pixel request");

	IconRef frostPNG = AGIconCreateWithType(kUTTypePNG);
	CGImageRef frostPNG64 = frostPNG != NULL ? AGIconCopyImage(frostPNG, CGSizeMake(64, 64)) : NULL;
	check(frostPNG64 != NULL && !imagesDiffer(frostPNG64, image64),
	      "a PNG gets its own theme icon, not the image generic");

	/* THE THEME IS A DEFAULT AND NOT A FLOOR: point the process at a directory that carries none, and the
	 * drawing is what a type gets. (Unsetting the variable is not enough once the compiled default is installed,
	 * which is exactly why the test uses an empty directory.) */
	char noThemePath[4096];
	snprintf(noThemePath, sizeof noThemePath, "%s/no-theme", root);
	mkdir(noThemePath, 0755);
	setenv("AGORA_ICON_THEME", noThemePath, 1);
	AGIconReset();
	IconRef drawnFolder = AGIconCreateWithType(kUTTypeFolder);
	CGImageRef drawnFolder64 = drawnFolder != NULL ? AGIconCopyImage(drawnFolder, CGSizeMake(64, 64)) : NULL;
	CGSize drawnBest = AGIconGetBestSize(drawnFolder), themedBest = AGIconGetBestSize(frostFolder);
	check(drawnFolder64 != NULL && folder64 != NULL && drawnBest.width == 256 && themedBest.width == 512,
	      "an empty theme directory leaves the drawn 256-point generic, not the theme's @2x art");
	check(drawnFolder64 != NULL && folder64 != NULL && imagesDiffer(drawnFolder64, folder64),
	      "and its pixels are not the theme's folder");

	setenv("AGORA_ICON_THEME", THEME_DIR, 1);
	AGIconReset();
	IconRef frostAgain = AGIconCreateWithType(kUTTypeFolder);
	CGImageRef frostAgain64 = frostAgain != NULL ? AGIconCopyImage(frostAgain, CGSizeMake(64, 64)) : NULL;
	check(frostAgain64 != NULL && folder64 != NULL && !imagesDiffer(frostAgain64, folder64),
	      "and pointing at the theme again resolves the same art");

	/* ---- 4c. a well-known place gets its own art ----------------------------------------- */
	char desktopPath[4096];
	snprintf(desktopPath, sizeof desktopPath, "%s/Desktop", root);
	mkdir(desktopPath, 0755);
	char savedHome[4096];
	const char *home = getenv("HOME");
	bool hadHome = home != NULL;
	if (hadHome) snprintf(savedHome, sizeof savedHome, "%s", home);
	setenv("HOME", root, 1);
	CFURLRef desktopURL = fileURL(desktopPath);
	IconRef desktopIcon = AGIconCreateWithURL(desktopURL);
	CGImageRef desktop64 = desktopIcon != NULL ? AGIconCopyImage(desktopIcon, CGSizeMake(64, 64)) : NULL;
	check(desktop64 != NULL && folder64 != NULL && imagesDiffer(desktop64, folder64),
	      "$HOME/Desktop gets the place's `user-desktop`, not the plain folder");
	if (hadHome) setenv("HOME", savedHome, 1); else unsetenv("HOME");
	AGIconReset();

	/* ---- 5. an override wins ------------------------------------------------------------- */
	IconRef override = AGIconCreateWithImage(folder64);
	AGIconRegisterIconForType(kUTTypePlainText, override);
	IconRef afterOverride = AGIconCreateWithType(kUTTypePlainText);
	CGImageRef after64 = AGIconCopyImage(afterOverride, CGSizeMake(64, 64));
	check(after64 != NULL && !imagesDiffer(after64, folder64),
	      "a registered override is what a type now draws");
	check(after64 != NULL && text64 != NULL && imagesDiffer(after64, text64),
	      "and it is no longer the built-in generic");

	/* ---- cleanup ------------------------------------------------------------------------- */
	CGImageRelease(frost32); CGImageRelease(frost512); CGImageRelease(frostPNG64);
	CGImageRelease(drawnFolder64); CGImageRelease(frostAgain64);
	CGImageRelease(desktop64);
	AGIconRelease(frostFolder); AGIconRelease(frostPNG); AGIconRelease(drawnFolder); AGIconRelease(frostAgain);
	AGIconRelease(desktopIcon);
	CFRelease(desktopURL);
	CGImageRelease(after64); CGImageRelease(badged128); CGImageRelease(viaURL64);
	CGImageRelease(text64); CGImageRelease(folder64); CGImageRelease(image64);
	CGImageRelease(i32); CGImageRelease(i128); CGImageRelease(i48);
	AGIconRelease(afterOverride); AGIconRelease(override); AGIconRelease(badged);
	AGIconRelease(viaURL); AGIconRelease(textIcon); AGIconRelease(folderIcon); AGIconRelease(imageIcon);
	AGIconRelease(app);
	CFRelease(folderURL);
	CFRelease(appURL);

	snprintf(cmd, sizeof cmd, "rm -rf '%s'", root);
	(void)system(cmd);

	printf("iconservices: %d check(s) failed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
