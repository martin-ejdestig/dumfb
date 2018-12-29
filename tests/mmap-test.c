// Copyright (C) 2018 Martin Ejdestig <marejde@gmail.com>
// SPDX-License-Identifier: GPL-2.0+

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct {
	int fd;
	struct fb_fix_screeninfo fix_info;
	struct fb_var_screeninfo var_info;
	uint8_t *pixels;
} fb_t;

#define FB_INIT { .fd = -1, .pixels = NULL }

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t;

#define RGB_WHITE { .r = 0xff, .g = 0xff, .b = 0xff }
#define RGB_RED { .r = 0xff, .g = 0x00, .b = 0x00 }
#define RGB_GREEN { .r = 0x00, .g = 0xff, .b = 0x00 }
#define RGB_BLUE { .r = 0x00, .g = 0x00, .b = 0xff }

#ifdef __GNUC__
# define ATTRIBUTE_PRINTF(f, a) __attribute__((format(printf, f, a)))
#else
# define ATTRIBUTE_PRINTF(f, a)
#endif

ATTRIBUTE_PRINTF(1, 2)
static void print_sys_error(const char *s, ...)
{
	va_list args;
	char str[1024];

	va_start(args, s);
	vsnprintf(str, sizeof(str), s, args);
	va_end(args);

	perror(str);
}

static void fb_close(fb_t *fb)
{
	if (fb->pixels) {
		if (munmap(fb->pixels, fb->fix_info.smem_len) != 0)
			print_sys_error("munmap failed");
		fb->pixels = NULL;
	}

	if (fb->fd != -1) {
		close(fb->fd);
		fb->fd = -1;
	}
}

static bool fb_open(fb_t *fb, const char *dev)
{
	assert(fb->fd == -1 && fb->pixels == NULL);

	fb->fd = open(dev, O_RDWR);
	if (fb->fd == -1) {
		print_sys_error("failed to open %s", dev);
		goto error;
	}

	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fix_info) == -1) {
		print_sys_error("FBIOGET_FSCREENINFO ioctl failed");
		goto error;
	}

	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->var_info) == -1) {
		print_sys_error("FBIOGET_VSCREENINFO ioctl failed");
		goto error;
	}

	void *ptr = mmap(NULL, fb->fix_info.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
	if (ptr == MAP_FAILED) {
		print_sys_error("failed to mmap %s", dev);
		goto error;
	}
	fb->pixels = ptr;

	return true;
error:
	fb_close(fb);
	return false;
}

static inline uint8_t lerp_u8(uint8_t u0, uint8_t u1, int num, int denom)
{
	return u0 + ((u1 - u0) * num) / denom;
}

static inline rgb_t lerp_rgb(rgb_t c0, rgb_t c1, int num, int denom)
{
	rgb_t c = { .r = lerp_u8(c0.r, c1.r, num, denom),
		    .g = lerp_u8(c0.g, c1.g, num, denom),
		    .b = lerp_u8(c0.b, c1.b, num, denom) };
	return c;
}

static bool draw_fb_format_supported(const fb_t *fb)
{
	if (fb->fix_info.visual != FB_VISUAL_TRUECOLOR)
		return false;

	if (fb->var_info.bits_per_pixel % 8 != 0 ||
	    fb->var_info.bits_per_pixel > 32)
		return false;

	return true;
}

static void draw_pixel(const fb_t *fb, int x, int y, rgb_t color)
{
	assert(draw_fb_format_supported(fb));

	// Slow and limted to 32, 24 and 16 bpp true color. But this is just a test...
	uint32_t r = color.r >> (8 - fb->var_info.red.length);
	uint32_t g = color.g >> (8 - fb->var_info.green.length);
	uint32_t b = color.b >> (8 - fb->var_info.blue.length);
	uint32_t bits = (r << fb->var_info.red.offset) |
			(g << fb->var_info.green.offset) |
			(b << fb->var_info.blue.offset);

	if (fb->var_info.transp.length > 0) {
		uint32_t mask = (1 << fb->var_info.transp.length) - 1;
		mask <<= fb->var_info.transp.offset;
		bits |= mask;
	}

	// TODO: Offsets etc...
	uint8_t *p = fb->pixels + y * fb->fix_info.line_length + x * fb->var_info.bits_per_pixel / 8;

	for (int i = (int) fb->var_info.bits_per_pixel - 8; i >= 0; i -= 8)
		*(p++) = (bits >> i) & 0xff;
}

static void draw_spread(const fb_t *fb)
{
	static const rgb_t top_left = RGB_WHITE;
	static const rgb_t top_right = RGB_RED;
	static const rgb_t bottom_left = RGB_GREEN;
	static const rgb_t bottom_right = RGB_BLUE;

	int h = fb->var_info.yres; // TODO: Offsets etc...
	int w = fb->var_info.xres;

	for (int y = 0; y < h; y++) {
		rgb_t left = lerp_rgb(top_left, bottom_left, y, h - 1);
		rgb_t right = lerp_rgb(top_right, bottom_right, y, h - 1);

		for (int x = 0; x < w; x++) {
			rgb_t c = lerp_rgb(left, right, x, w - 1);
			draw_pixel(fb, x, y, c);
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <fb dev>\n", argv[0]);
		return EXIT_SUCCESS;
	}

	fb_t fb = FB_INIT;
	if (!fb_open(&fb, argv[1]))
		return EXIT_FAILURE;

	if (draw_fb_format_supported(&fb))
		draw_spread(&fb);
	else
		printf("error: framebuffer format not supported\n");

	fb_close(&fb);

	return EXIT_SUCCESS;
}
