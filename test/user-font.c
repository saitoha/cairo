/*
 * Copyright © 2006, 2008 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 *	Behdad Esfahbod <behdad@behdad.org>
 */

#include <stdlib.h>
#include <stdio.h>

#include "cairo-test.h"

/*#define ROTATED 1*/

#define BORDER 10
#define TEXT_SIZE 64
#define WIDTH  (TEXT_SIZE * 15 + 2*BORDER)
#define HEIGHT ((TEXT_SIZE + 2*BORDER)*2)
#define TEXT   "geez... cairo user-font"

static cairo_test_draw_function_t draw;

cairo_test_t test = {
    "user-font",
    "Tests user font feature",
#ifndef ROTATED
    WIDTH, HEIGHT,
#else
    WIDTH, WIDTH,
#endif
    draw
};

#define END_GLYPH 0
#define STROKE 126
#define CLOSE 127

/* Simple glyph definition: 1 - 15 means lineto (or moveto for first
 * point) for one of the points on this grid:
 *
 *      1  2  3
 *      4  5  6
 *      7  8  9
 * ----10 11 12----(baseline)
 *     13 14 15
 */

static const struct {
    unsigned long ucs4;
    int width;
    char data[16];
} glyphs [] = {
    { '\0', 0, { END_GLYPH } },
    { ' ',  1, { END_GLYPH } },
    { '-',  2, { 7, 8, STROKE, END_GLYPH } },
    { '.',  1, { 10, 10, STROKE, END_GLYPH } },
    { 'a',  3, { 4, 6, 12, 10, 7, 9, STROKE, END_GLYPH } },
    { 'c',  3, { 6, 4, 10, 12, STROKE, END_GLYPH } },
    { 'e',  3, { 12, 10, 4, 6, 9, 7, STROKE, END_GLYPH } },
    { 'f',  3, { 3, 2, 11, STROKE, 4, 6, STROKE, END_GLYPH } },
    { 'g',  3, { 12, 10, 4, 6, 15, 13, STROKE, END_GLYPH } },
    { 'h',  3, { 1, 10, STROKE, 7, 5, 6, 12, STROKE, END_GLYPH } },
    { 'i',  1, { 1, 1, STROKE, 4, 10, STROKE, END_GLYPH } },
    { 'l',  1, { 1, 10, STROKE, END_GLYPH } },
    { 'n',  3, { 10, 4, STROKE, 7, 5, 6, 12, STROKE, END_GLYPH } },
    { 'o',  3, { 4, 10, 12, 6, CLOSE, END_GLYPH } },
    { 'r',  3, { 4, 10, STROKE, 7, 5, 6, STROKE, END_GLYPH } },
    { 's',  3, { 6, 4, 7, 9, 12, 10, STROKE, END_GLYPH } },
    { 't',  3, { 2, 11, 12, STROKE, 4, 6, STROKE, END_GLYPH } },
    { 'u',  3, { 4, 10, 12, 6, STROKE, END_GLYPH } },
    { 'z',  3, { 4, 6, 10, 12, STROKE, END_GLYPH } },
    {  -1,  0, { END_GLYPH } },
};

static cairo_status_t
test_scaled_font_init (cairo_scaled_font_t  *scaled_font,
		       cairo_font_extents_t *metrics)
{
  metrics->ascent  = .75;
  metrics->descent = .25;
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
test_scaled_font_unicode_to_glyph (cairo_scaled_font_t *scaled_font,
				   unsigned long        unicode,
				   unsigned long       *glyph)
{
    int i;

    for (i = 0; glyphs[i].ucs4 != -1; i++)
	if (glyphs[i].ucs4 == unicode) {
	    *glyph = i;
	    return CAIRO_STATUS_SUCCESS;
	}

    /* Fall through and default to undefined glyph. */
    return CAIRO_STATUS_INVALID_INDEX;
}

static cairo_status_t
test_scaled_font_render_glyph (cairo_scaled_font_t  *scaled_font,
			       unsigned long         glyph,
			       cairo_t              *cr,
			       cairo_text_extents_t *metrics)
{
    int i;
    const char *data;
    div_t d;
    double x, y;

    metrics->x_advance = glyphs[glyph].width / 4.0;

    cairo_set_line_width (cr, 0.1);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

    data = glyphs[glyph].data;
    for (i = 0; data[i] != END_GLYPH; i++) {
	switch (data[i]) {
	case STROKE:
	    cairo_new_sub_path (cr);
	    break;

	case CLOSE:
	    cairo_close_path (cr);
	    break;

	default:
	    d = div (data[i] - 1, 3);
	    x = d.rem / 4.0 + 0.125;
	    y = d.quot / 5.0 + 0.4 - 1.0;
	    cairo_line_to (cr, x, y);
	}
    }
    cairo_stroke (cr);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_font_face_t *
get_user_font_face (void)
{
    static cairo_font_face_t *user_font_face = NULL;

    if (!user_font_face) {
	user_font_face = cairo_user_font_face_create ();
	cairo_user_font_face_set_init_func             (user_font_face, test_scaled_font_init);
	cairo_user_font_face_set_render_glyph_func     (user_font_face, test_scaled_font_render_glyph);
	cairo_user_font_face_set_unicode_to_glyph_func (user_font_face, test_scaled_font_unicode_to_glyph);
    }

    return user_font_face;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    const char text[] = TEXT;
    cairo_font_extents_t font_extents;
    cairo_text_extents_t extents;

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

#ifdef ROTATED
    cairo_translate (cr, TEXT_SIZE, 0);
    cairo_rotate (cr, .6);
#endif

    cairo_set_font_face (cr, get_user_font_face ());
    cairo_set_font_size (cr, TEXT_SIZE);

    cairo_font_extents (cr, &font_extents);
    cairo_text_extents (cr, text, &extents);

    /* logical boundaries in red */
    cairo_move_to (cr, 0, BORDER);
    cairo_rel_line_to (cr, WIDTH, 0);
    cairo_move_to (cr, 0, BORDER + font_extents.ascent);
    cairo_rel_line_to (cr, WIDTH, 0);
    cairo_move_to (cr, 0, BORDER + font_extents.ascent + font_extents.descent);
    cairo_rel_line_to (cr, WIDTH, 0);
    cairo_move_to (cr, BORDER, 0);
    cairo_rel_line_to (cr, 0, 2*BORDER + TEXT_SIZE);
    cairo_move_to (cr, BORDER + extents.x_advance, 0);
    cairo_rel_line_to (cr, 0, 2*BORDER + TEXT_SIZE);
    cairo_set_source_rgba (cr, 1, 0, 0, .8);
    cairo_set_line_width (cr, 2);
    cairo_stroke (cr);

    /* ink boundaries in green */
    cairo_rectangle (cr,
		     BORDER + extents.x_bearing, BORDER + font_extents.ascent + extents.y_bearing,
		     extents.width, extents.height);
    cairo_set_source_rgba (cr, 0, 1, 0, .8);
    cairo_set_line_width (cr, 2);
    cairo_stroke (cr);

    /* text in gray */
    cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
    cairo_move_to (cr, BORDER, BORDER + font_extents.ascent);
    cairo_show_text (cr, text);


    /* filled version of text in light blue */
    cairo_set_source_rgba (cr, 0, 0, 1, 0.5);
    cairo_move_to (cr, BORDER, BORDER + font_extents.height + 2*BORDER + font_extents.ascent);
    cairo_text_path (cr, text);
    cairo_fill (cr);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}
