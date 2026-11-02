/*
 * iconservices.c — F2's measurement.
 *
 * Copyright © 2026 Kyle J. Cardoza.
 * SPDX-FileCopyrightText: 2026 Kyle J. Cardoza
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * WHAT THIS HOLDS UP, in WORKSPACE-PLAN.md's words for F2:
 *
 *   * an application's DECLARED icon is found (its `Info.plist` names a `.icns`, the reader finds the PNG
 *     representations inside it, and the pixels that come back are the ones the fixture declared);
 *   * the icon is available AT TWO SIZES and a size with no representation is scaled;
 *   * a text type and a folder get DIFFERENT icons, so a type really picks its own;
 *   * a badge composites over a base without changing its size;
 *   * an override registered for a type wins;
 *   * and a theme is the default: a type resolves through one at both scales, while pointing `AGORA_ICON_THEME` at an
 *     empty directory brings the drawn generic back. THE THEME IS COMPOSED BY THIS FILE — an `index.theme` and a few
 *     flat-coloured PNGs, with the checkout's `Tabler` linked beside it — so the suite measures the lookup and not
 *     anybody's art, and the component ships none.
 *
 * THE FIXTURES ARE NOT BINARY FILES IN THE TREE. Two solid-colour PNGs are embedded as base64 and the test
 * composes an `.icns` container around them, so the reader is measured against a container this file built.
 */
#define _DEFAULT_SOURCE

#include <IconServices/IconServices.h>
#include <LaunchServices/UTType.h>

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGImageDestination.h>
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

/* ---- composing an icon theme, so the suite needs no art of its own ---------------------------- */

/* A solid rectangle, at the size asked. It is a THEME's picture for these checks, and a colour is the point: the entry
 * that answered a request is named by what came back, not by comparing against a picture committed to the tree. */
static CGImageRef solidImage(size_t w, size_t h, int r, int g, int b)
{
	CGContextRef ctx = rasterContext(w, h);
	CGImageRef image;

	if (ctx == NULL) return NULL;
	CGContextSetRGBFillColor(ctx, (CGFloat)r / 255.0, (CGFloat)g / 255.0, (CGFloat)b / 255.0, 1.0);
	CGContextFillRect(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h));
	image = CGBitmapContextCreateImage(ctx);
	CGContextRelease(ctx);
	return image;
}

/* The picture as PNG bytes, through the tree's own encoder. */
static CFDataRef pngData(CGImageRef image)
{
	CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
	CFStringRef type = CFStringCreateWithCString(NULL, "public.png", kCFStringEncodingUTF8);
	CGImageDestinationRef dest = CGImageDestinationCreateWithData(data, type, 1, NULL);
	CFDataRef answer;

	CFRelease(type);
	if (dest == NULL) {
		CFRelease(data);
		return NULL;
	}
	CGImageDestinationAddImage(dest, image, NULL);
	(void)CGImageDestinationFinalize(dest);
	CFRelease(dest);
	answer = CFDataCreateCopy(NULL, data);
	CFRelease(data);
	return answer;
}

/* One entry of the composed theme: `<theme>/<subdir>/<name>.png`, made on the way. */
static bool writeThemeImage(const char *theme, const char *subdir, const char *name, CGImageRef image)
{
	char dir[4096], path[4096], command[4096];
	CFDataRef bytes;
	bool ok;

	snprintf(dir, sizeof dir, "%s/%s", theme, subdir);
	snprintf(command, sizeof command, "mkdir -p '%s'", dir);
	if (system(command) != 0) return false;
	snprintf(path, sizeof path, "%s/%s.png", dir, name);
	bytes = pngData(image);
	if (bytes == NULL) return false;
	ok = writeFile(path, bytes);
	CFRelease(bytes);
	return ok;
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

	/* **THE THEME THE SUITE MEASURES IS ONE IT COMPOSED.** It lives in its own `icons/` directory with the checkout's
	 * `Tabler` linked beside it, because the sibling rule is part of what is measured: a name the theme in charge does
	 * not carry is asked of `Tabler` next door. Nothing here is installed, and the component ships no art. */
	char themePath[4096], iconsPath[4096], tablerLink[4096];
	snprintf(iconsPath, sizeof iconsPath, "%s/icons", root);
	snprintf(themePath, sizeof themePath, "%s/Composed", iconsPath);
	snprintf(tablerLink, sizeof tablerLink, "%s/Tabler", iconsPath);
	{
		char command[4096];

		snprintf(command, sizeof command, "mkdir -p '%s'", themePath);
		if (system(command) != 0) { fprintf(stderr, "theme directory failed\n"); return 1; }
		(void)symlink(THEMES_DIR "/Tabler", tablerLink);
	}

	setenv("AGORA_ICON_THEME", themePath, 1);
	AGIconReset();
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

	/* ---- 1a. A BUNDLE THAT NAMES NO ICON STILL HAS ONE -------------------------------------------
	 *
	 * "Workspace does not recognise .app bundles in the trash" was this hole: a bundle whose manifest names nothing —
	 * an older build, or one written by hand — answered NULL and the caller drew the theme's generic application
	 * glyph. The search now reads the conventional names and then the one picture the bundle ships, which is what
	 * these two cases measure: an `icon.png` nobody named, and a lone `.icns` whose name is the bundle's own. */
	{
		char plainPath[4096], lonePath[4096], command[4096];
		CFURLRef url;
		IconRef found;
		CGImageRef pixels;

		/* (a) NO CFBundleIconFile, AN UNNAMED `icon.png`. */
		snprintf(plainPath, sizeof plainPath, "%s/Plain.app", root);
		snprintf(command, sizeof command,
			 "mkdir -p '%s/Contents/Resources' && printf '%%s' "
			 "'<plist version=\"1.0\"><dict><key>CFBundleName</key><string>Plain</string></dict></plist>' "
			 "> '%s/Contents/Info.plist'", plainPath, plainPath);
		check(system(command) == 0, "a bundle with no CFBundleIconFile is made");
		{
			CGImageRef art = solidImage(64, 64, 30, 200, 120);
			CFDataRef bytes = pngData(art);

			snprintf(command, sizeof command, "%s/Plain.app/Contents/Resources/icon.png", root);
			check(writeFile(command, bytes), "carrying an icon.png nobody named");
			CFRelease(bytes);
			CGImageRelease(art);
		}
		url = fileURL(plainPath);
		found = AGIconCreateWithURL(url);
		CFRelease(url);
		pixels = found != NULL ? AGIconCopyImage(found, CGSizeMake(64, 64)) : NULL;
		check(pixels != NULL && imageHasColour(pixels, 30, 200, 120, 8),
		      "and the bundle's own picture is what resolves, not the generic application glyph");
		if (pixels != NULL) CGImageRelease(pixels);
		if (found != NULL) AGIconRelease(found);

		/* (b) NO Resources NAMES AT ALL, A LONE `.icns` NAMED AFTER THE BUNDLE. */
		snprintf(lonePath, sizeof lonePath, "%s/Lone.app", root);
		snprintf(command, sizeof command,
			 "mkdir -p '%s/Contents/Resources' && printf '%%s' "
			 "'<plist version=\"1.0\"><dict><key>CFBundleName</key><string>Lone</string></dict></plist>' "
			 "> '%s/Contents/Info.plist'", lonePath, lonePath);
		check(system(command) == 0, "a bundle whose picture is named after it is made");
		{
			CGImageRef art = solidImage(128, 128, 220, 60, 60);
			CFDataRef archive = pngData(art);

			snprintf(command, sizeof command, "%s/Lone.app/Contents/Resources/Lone.icns", root);
			/* The `.icns` reader takes a PNG-bearing container, so the picture is packed the way it reads one. */
			check(archive != NULL, "the bundle's artwork is made");
			if (archive != NULL) {
				CFDataRef icns = makeICNS(archive, archive);

				check(writeFile(command, icns), "packed into a .icns named after the bundle");
				CFRelease(icns);
				CFRelease(archive);
			}
			CGImageRelease(art);
		}
		url = fileURL(lonePath);
		found = AGIconCreateWithURL(url);
		CFRelease(url);
		pixels = found != NULL ? AGIconCopyImage(found, CGSizeMake(64, 64)) : NULL;
		check(pixels != NULL && imageHasColour(pixels, 220, 60, 60, 8),
		      "and a lone .icns is found by the bundle's own name");
		if (pixels != NULL) CGImageRelease(pixels);
		if (found != NULL) AGIconRelease(found);
		/* (c) A PICTURE WITH A NAME NEITHER THE PLIST NOR THE CONVENTION GIVES — the third pass, which reads
		 * `Contents/Resources` and takes the one `.icns` it finds. Nothing else could answer this one. */
		{
			char oddPath[4096];

			snprintf(oddPath, sizeof oddPath, "%s/Odd.app", root);
			snprintf(command, sizeof command,
				 "mkdir -p '%s/Contents/Resources' && printf '%%s' "
				 "'<plist version=\"1.0\"><dict><key>CFBundleName</key><string>Odd</string></dict></plist>' "
				 "> '%s/Contents/Info.plist'", oddPath, oddPath);
			check(system(command) == 0, "a bundle whose picture is named nothing like it is made");
			{
				CGImageRef art = solidImage(96, 96, 90, 60, 200);
				CFDataRef archive = pngData(art);

				if (archive != NULL) {
					CFDataRef icns = makeICNS(archive, archive);

					snprintf(command, sizeof command, "%s/Odd.app/Contents/Resources/Whatever.icns", root);
					check(writeFile(command, icns), "as CoolArt.icns, a name neither the plist nor the bundle gives");
					CFRelease(icns);
					CFRelease(archive);
				}
				CGImageRelease(art);
			}
			url = fileURL(oddPath);
			found = AGIconCreateWithURL(url);
			CFRelease(url);
			pixels = found != NULL ? AGIconCopyImage(found, CGSizeMake(64, 64)) : NULL;
			check(pixels != NULL && imageHasColour(pixels, 90, 60, 200, 8),
			      "and the one picture the bundle ships is the one it means");
			if (pixels != NULL) CGImageRelease(pixels);
			if (found != NULL) AGIconRelease(found);
		}

		{
			char cleanup[4096];

			snprintf(cleanup, sizeof cleanup, "rm -rf '%s/Plain.app' '%s/Lone.app' '%s/Odd.app'", root, root, root);
			(void)system(cleanup);
		}
	}

	/* ---- 1b. A PICTURE FINER THAN THE SIZE ASKED FOR IS AVERAGED, NOT SAMPLED -----------------
	 *
	 * A one-pixel checkerboard reduced 8:1 has no honest single sample: nearest returns black or white and the
	 * picture arrives with stair-stepped edges, which is exactly what an application's 512-pixel artwork looked like
	 * at a 64-point icon. Averaging any 8x8 block of this pattern gives the SAME number everywhere — 127 — so the
	 * tolerance below is the difference between a sampler and a picker, and it cannot pass by accident. */
	{
		const size_t S = 512, T = 64;
		CGContextRef big = rasterContext(S, S);
		unsigned char *px = CGBitmapContextGetData(big);
		bool averaged = false;

		check(big != NULL, "a texture to reduce is made");
		if (big != NULL) {
			for (size_t y = 0; y < S; y++) {
				for (size_t x = 0; x < S; x++) {
					unsigned char v = ((x + y) & 1) ? 255 : 0;

					px[(y * S + x) * 4 + 0] = v;
					px[(y * S + x) * 4 + 1] = v;
					px[(y * S + x) * 4 + 2] = v;
					px[(y * S + x) * 4 + 3] = 255;
				}
			}{
				CGImageRef checker = CGBitmapContextCreateImage(big);
				IconRef ref = AGIconCreateWithImage(checker);
				CGImageRef small = AGIconCopyImage(ref, CGSizeMake((CGFloat)T, (CGFloat)T));

				averaged = imageHasColour(small, 127, 127, 127, 28);
				check(small != NULL && CGImageGetWidth(small) == T,
				      "a 512-pixel picture asked for at 64 comes back at 64");
				check(averaged, "and its pixels are AVERAGED (a mid grey), not one source pixel picked");
				CGImageRelease(small);
				AGIconRelease(ref);
				CGImageRelease(checker);
			}
			CGContextRelease(big);
		}
	}

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

	/* ---- 4b. a theme composed by this file, and the drawing it replaces ------------------- */

	/* **THE THEME IS COMPOSED HERE, AND IT IS WHY THIS SUITE OWNS NO ART.** The component used to vendor one theme and
	 * measure against its pixels; the theme a session actually runs is installed beside the prefix or rendered from an
	 * upstream checkout, so what belongs in a test is the MECHANISM — a name and a size resolved to the representation
	 * that carries them, and the drawn generic when nothing does. Each entry here is one flat colour, so the entry that
	 * answered a request is named by what came back. */
	char indexPath[4096];
	snprintf(indexPath, sizeof indexPath, "%s/index.theme", themePath);
	{
		CGImageRef art32 = solidImage(32, 32, 220, 60, 60);
		CGImageRef art512 = solidImage(512, 512, 60, 90, 210);
		CGImageRef artPNG = solidImage(64, 64, 40, 170, 90);
		CFDataRef index = (CFDataRef)CFStringCreateExternalRepresentation(NULL,
			CFSTR("[Icon Theme]\nName=Composed\n"), kCFStringEncodingUTF8, 0);

		check(writeFile(indexPath, index), "the composed theme carries an index.theme");
		CFRelease(index);
		check(writeThemeImage(themePath, "32x32/places", "folder", art32),
		      "a folder at 32 pixels is in it");
		check(writeThemeImage(themePath, "256x256@2x/places", "folder", art512),
		      "with a @2x representation at 512");
		check(writeThemeImage(themePath, "64x64/mimetypes", "image-png", artPNG),
		      "and a PNG type's own picture");
		/* **THE NAMES THE REST OF THE SUITE ASKS FOR.** `image-x-generic` is the same picture as the PNG's — which is
		 * how a theme that draws one bitmap for a family says it — and the two place names are what the sibling rule and
		 * the theme prefix are measured against: `folder-open` must be a DIFFERENT picture from Tabler's outline symbol
		 * at a different size, so "the theme in charge wins" is visible. */
		check(writeThemeImage(themePath, "64x64/mimetypes", "image-x-generic", artPNG),
		      "and the image generic it belongs to");
		{
			CGImageRef place = solidImage(48, 48, 60, 170, 90);
			CGImageRef open = solidImage(48, 48, 200, 150, 40);

			check(writeThemeImage(themePath, "48x48/places", "user-desktop", place),
			      "a place is one of its names");
			check(writeThemeImage(themePath, "48x48/places", "folder-open", open),
			      "and so is a folder-open, drawn unlike the symbol theme's");
			CGImageRelease(place);
			CGImageRelease(open);
		}
		CGImageRelease(art32);
		CGImageRelease(art512);
		CGImageRelease(artPNG);
	}

	setenv("AGORA_ICON_THEME", themePath, 1);
	AGIconReset();

	IconRef themeFolder = AGIconCreateWithType(kUTTypeFolder);
	CGImageRef theme32 = themeFolder != NULL ? AGIconCopyImage(themeFolder, CGSizeMake(32, 32)) : NULL;
	CGImageRef theme512 = themeFolder != NULL ? AGIconCopyImage(themeFolder, CGSizeMake(512, 512)) : NULL;
	check(themeFolder != NULL && theme32 != NULL && CGImageGetWidth(theme32) == 32,
	      "a type resolves through the theme at the size asked");
	check(theme32 != NULL && imageHasColour(theme32, 220, 60, 60, 8),
	      "and it is that size's picture, not another size's");
	check(theme512 != NULL && CGImageGetWidth(theme512) == 512,
	      "and the theme's @2x representation serves a 512-pixel request");
	check(theme512 != NULL && imageHasColour(theme512, 60, 90, 210, 8),
	      "which is the @2x art rather than a scaled-up 32");

	IconRef themePNG = AGIconCreateWithType(kUTTypePNG);
	CGImageRef themePNG64 = themePNG != NULL ? AGIconCopyImage(themePNG, CGSizeMake(64, 64)) : NULL;
	check(themePNG64 != NULL && CGImageGetWidth(themePNG64) == 64,
	      "a PNG type resolves through the theme too");

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
	CGSize drawnBest = AGIconGetBestSize(drawnFolder), themedBest = AGIconGetBestSize(themeFolder);
	check(drawnFolder64 != NULL && folder64 != NULL && drawnBest.width == 256 && themedBest.width == 512,
	      "an empty theme directory leaves the drawn 256-point generic, not the theme's @2x art");
	check(drawnFolder64 != NULL && folder64 != NULL && imagesDiffer(drawnFolder64, folder64),
	      "and its pixels are not the theme's folder");
	/* **AND A TYPE GETS THE THEME'S PICTURE RATHER THAN THE DRAWN GENERIC** — measured while no theme is in charge, so
	 * the comparison is against the drawing and not against the theme twice. */
	{
		IconRef drawnPNG = AGIconCreateWithType(kUTTypePNG);
		CGImageRef drawnPNG64 = drawnPNG != NULL ? AGIconCopyImage(drawnPNG, CGSizeMake(64, 64)) : NULL;

		check(drawnPNG64 != NULL && themePNG64 != NULL && imagesDiffer(drawnPNG64, themePNG64),
		      "a PNG gets its own theme icon, not the image generic");
		if (drawnPNG64 != NULL) CGImageRelease(drawnPNG64);
		AGIconRelease(drawnPNG);
	}

	setenv("AGORA_ICON_THEME", themePath, 1);
	AGIconReset();
	IconRef themeAgain = AGIconCreateWithType(kUTTypeFolder);
	CGImageRef themeAgain64 = themeAgain != NULL ? AGIconCopyImage(themeAgain, CGSizeMake(64, 64)) : NULL;
	check(themeAgain64 != NULL && folder64 != NULL
	      && imageHasColour(themeAgain64, 60, 90, 210, 8),
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

	/* ---- 4d. **A SYMBOL IS ASKED FOR BY NAME, AND IT LIVES IN THE THEME BESIDE THIS ONE.** The theme in charge is
	 * places, file types and application art; the UI's symbols are Tabler, installed as its own theme. `AGORA_ICON_THEME`
	 * here names the composed theme, so `hammer` can only be answered by the SIBLING lookup — which is the whole of
	 * "Tabler beside the theme in charge", and what makes `[NSImage imageNamed:@"hammer"]` a system resource rather than
	 * nine tables compiled into one program. */
	{
		IconRef symbol = AGIconCreateWithName(CFSTR("hammer"));
		CGSize natural = AGIconGetNaturalSize(symbol);
		CGSize best = AGIconGetBestSize(symbol);
		CGImageRef at24 = symbol != NULL ? AGIconCopyImage(symbol, CGSizeMake(24, 24)) : NULL;

		check(symbol != NULL, "a symbol is found BY NAME, in the theme beside the one in charge");
		check(natural.width == 24 && natural.height == 24,
		      "and its natural size is the 24-point grid a Tabler symbol is drawn on");
		check(best.width == 96 && best.height == 96, "while its best painting is the 96-pixel one");
		check(at24 != NULL && CGImageGetWidth(at24) == 24 && CGImageGetHeight(at24) == 24,
		      "and the 24-pixel representation comes back at 24x24");
		if (at24 != NULL) CGImageRelease(at24);
		AGIconRelease(symbol);

		/* A NAME THE THEME IN CHARGE CARRIES IS STILL FOUND IN IT FIRST, so the fallback adds nothing to the cost of
		 * the ordinary lookups: `folder` and `user-desktop` are the composed theme's. */
		IconRef place = AGIconCreateWithName(CFSTR("user-desktop"));
		CGSize placeNatural = AGIconGetNaturalSize(place);

		check(place != NULL && placeNatural.width > 0,
		      "and a name the theme IN CHARGE carries is answered from it, not from the sibling");
		AGIconRelease(place);

		/* **AND A NAME IS A NAME, NOT A PATH.** The lookup refuses anything that could leave the theme directory. */
		check(AGIconCreateWithName(CFSTR("../../etc/passwd")) == NULL, "a name that walks upwards resolves nothing");
		check(AGIconCreateWithName(CFSTR("places/folder")) == NULL, "and neither does one carrying a separator");
		check(AGIconCreateWithName(CFSTR("no-such-symbol-anywhere")) == NULL,
		      "nor one no theme carries, which leaves the caller's own drawing in charge");

		/* **AND A NAME MAY SAY WHICH THEME IT MEANS, WHICH IS WHAT A TOOLBAR USES.** `folder-open` is a name BOTH themes
		 * carry — a filled, coloured place folder and Tabler's outline symbol — so the prefix settles it, and
		 * `toolbar:` is the alias for whichever theme holds the UI's symbols. Unadorned names still ask the theme in charge
		 * (Frost here), which is what a file view wants. MEASURED: Forge's Open row drew Frost's solid folder in a template
		 * band until it asked `toolbar:folder-open`. */
		{
			IconRef bare = AGIconCreateWithName(CFSTR("folder-open"));
			IconRef aliased = AGIconCreateWithName(CFSTR("toolbar:folder-open"));
			IconRef named = AGIconCreateWithName(CFSTR("tabler:folder-open"));
			IconRef caseFold = AGIconCreateWithName(CFSTR("Tabler:folder-open"));
			CGSize aliasedSize = AGIconGetNaturalSize(aliased);
			CGImageRef aliasPixels = aliased != NULL ? AGIconCopyImage(aliased, CGSizeMake(24, 24)) : NULL;
			CGImageRef namedPixels = named != NULL ? AGIconCopyImage(named, CGSizeMake(24, 24)) : NULL;
			CGImageRef barePixels = bare != NULL ? AGIconCopyImage(bare, CGSizeMake(24, 24)) : NULL;

			check(bare != NULL && AGIconGetNaturalSize(bare).width != aliasedSize.width,
			      "a BARE name is the theme in charge's picture, which is not the symbol theme's");
			check(aliased != NULL && aliasedSize.width == 24.0 && aliasedSize.height == 24.0,
			      "`toolbar:` answers the SYMBOL theme's 24-pixel outline");
			check(aliasPixels != NULL && namedPixels != NULL && !imagesDiffer(aliasPixels, namedPixels),
			      "and it is the same picture `tabler:` names, because the alias IS that theme");
			check(caseFold != NULL, "a theme prefix is matched without case");
			check(aliasPixels != NULL && barePixels != NULL && imagesDiffer(aliasPixels, barePixels),
			      "while the bare name's painting is a different picture — the place folder, not the symbol");
			check(AGIconCreateWithName(CFSTR("no-such-theme:folder-open")) == NULL,
			      "and a prefix that names NO theme answers nothing, rather than guessing another theme's art");
			if (aliasPixels != NULL) CGImageRelease(aliasPixels);
			if (namedPixels != NULL) CGImageRelease(namedPixels);
			if (barePixels != NULL) CGImageRelease(barePixels);
			AGIconRelease(bare);
			AGIconRelease(aliased);
			AGIconRelease(named);
			AGIconRelease(caseFold);
		}
	}

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
	CGImageRelease(theme32); CGImageRelease(theme512); CGImageRelease(themePNG64);
	CGImageRelease(drawnFolder64); CGImageRelease(themeAgain64);
	CGImageRelease(desktop64);
	AGIconRelease(themeFolder); AGIconRelease(themePNG); AGIconRelease(drawnFolder); AGIconRelease(themeAgain);
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
