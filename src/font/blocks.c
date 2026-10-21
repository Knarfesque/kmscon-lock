/*
 * kmscon - Block Elements
 *
 * Copyright (c) 2026 Red Hat.
 * Author: Jocelyn Falempe <jfalempe@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * SECTION:blocks.c
 * @short_description: Block Elements drawing
 * @include: font.h
 *
 * The Unicode Block Elements (U+2580 to U+259F) are drawn directly, without
 * using the font backend. This way they always fill the whole cell, so they
 * join seamlessly with their neighbours, whatever the font in use.
 */

#include <stdlib.h>
#include "font.h"

#define BLOCKS_FIRST 0x2580
#define BLOCKS_LAST 0x259f

#define QUAD_UL 0x1
#define QUAD_UR 0x2
#define QUAD_LL 0x4
#define QUAD_LR 0x8

/* Quadrants from U+2596 to U+259F */
static const uint8_t quadrants[] = {
	QUAD_LL,		     // ▖
	QUAD_LR,		     // ▗
	QUAD_UL,		     // ▘
	QUAD_UL | QUAD_LL | QUAD_LR, // ▙
	QUAD_UL | QUAD_LR,	     // ▚
	QUAD_UL | QUAD_UR | QUAD_LL, // ▛
	QUAD_UL | QUAD_UR | QUAD_LR, // ▜
	QUAD_UR,		     // ▝
	QUAD_UR | QUAD_LL,	     // ▞
	QUAD_UR | QUAD_LL | QUAD_LR, // ▟
};

/* Returns n eighths of size, rounded, and at least one pixel */
static unsigned int eighths(unsigned int size, unsigned int n)
{
	unsigned int val = (size * n + 4) / 8;

	return val ? val : 1;
}

static void fill(struct kmscon_glyph *glyph, unsigned int x0, unsigned int y0, unsigned int x1,
		 unsigned int y1, uint8_t val)
{
	unsigned int x, y;

	for (y = y0; y < y1; y++)
		for (x = x0; x < x1; x++)
			glyph->buf.data[y * glyph->buf.width + x] = val;
}

static void draw_quadrants(struct kmscon_glyph *glyph, uint8_t quad)
{
	unsigned int w = glyph->buf.width;
	unsigned int h = glyph->buf.height;

	if (quad & QUAD_UL)
		fill(glyph, 0, 0, w / 2, h / 2, 0xff);
	if (quad & QUAD_UR)
		fill(glyph, w / 2, 0, w, h / 2, 0xff);
	if (quad & QUAD_LL)
		fill(glyph, 0, h / 2, w / 2, h, 0xff);
	if (quad & QUAD_LR)
		fill(glyph, w / 2, h / 2, w, h, 0xff);
}

/**
 * kmscon_font_blocks_has_glyph:
 * @ch: Symbol to check
 *
 * Returns: true if @ch is a Block Element, drawn by kmscon_font_blocks_render()
 */
bool kmscon_font_blocks_has_glyph(uint32_t ch)
{
	return ch >= BLOCKS_FIRST && ch <= BLOCKS_LAST;
}

/**
 * kmscon_font_blocks_render:
 * @font: Font object, used for the cell size
 * @ch: Block Element to draw
 *
 * Draws the Block Element @ch, with the cell size of @font.
 *
 * Returns: a new allocated glyph object on success, NULL on failure
 */
struct kmscon_glyph *kmscon_font_blocks_render(struct kmscon_font *font, uint32_t ch)
{
	struct kmscon_glyph *glyph;
	unsigned int w = font->width;
	unsigned int h = font->height;

	if (!kmscon_font_blocks_has_glyph(ch) || !w || !h)
		return NULL;

	glyph = calloc(1, sizeof(*glyph) + w * h);
	if (!glyph)
		return NULL;

	glyph->double_width = false;
	glyph->buf.width = w;
	glyph->buf.height = h;

	switch (ch) {
	case 0x2580: // ▀ Upper Half Block
		fill(glyph, 0, 0, w, h / 2, 0xff);
		break;
	case 0x2581 ... 0x2588: // ▁ to █ Lower One Eighth Block to Full Block
		fill(glyph, 0, h - eighths(h, ch - 0x2580), w, h, 0xff);
		break;
	case 0x2589 ... 0x258f: // ▉ to ▏ Left Seven Eighths Block to Left One Eighth Block
		fill(glyph, 0, 0, eighths(w, 0x2590 - ch), h, 0xff);
		break;
	case 0x2590: // ▐ Right Half Block
		fill(glyph, w / 2, 0, w, h, 0xff);
		break;
	case 0x2591: // ░ Light Shade
		fill(glyph, 0, 0, w, h, 0x40);
		break;
	case 0x2592: // ▒ Medium Shade
		fill(glyph, 0, 0, w, h, 0x80);
		break;
	case 0x2593: // ▓ Dark Shade
		fill(glyph, 0, 0, w, h, 0xc0);
		break;
	case 0x2594: // ▔ Upper One Eighth Block
		fill(glyph, 0, 0, w, eighths(h, 1), 0xff);
		break;
	case 0x2595: // ▕ Right One Eighth Block
		fill(glyph, w - eighths(w, 1), 0, w, h, 0xff);
		break;
	default: // ▖ to ▟ Quadrants
		draw_quadrants(glyph, quadrants[ch - 0x2596]);
		break;
	}
	return glyph;
}
