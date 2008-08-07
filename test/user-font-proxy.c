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
#define WIDTH  (TEXT_SIZE * 12 + 2*BORDER)
#define HEIGHT ((TEXT_SIZE + 2*BORDER)*2)
#define TEXT   "geez... cairo user-font"

static cairo_test_draw_function_t draw;

cairo_test_t test = {
    "user-font-proxy",
    "Tests a user-font using a native font in its render_glyph",
#ifndef ROTATED
    WIDTH, HEIGHT,
#else
    WIDTH, WIDTH,
#endif
    draw
};

static cairo_user_data_key_t fallback_font_face_key;

static cairo_status_t
test_scaled_font_init (cairo_scaled_font_t  *scaled_font,
		       cairo_font_extents_t *extents)
{
  cairo_font_face_t *font_face;
  cairo_matrix_t font_matrix, ctm;
  cairo_font_options_t *font_options;
  cairo_scaled_font_t *fallback_scaled_font;

  font_face = cairo_toy_font_face_create ("",
					  CAIRO_FONT_SLANT_NORMAL,
					  CAIRO_FONT_WEIGHT_NORMAL);

  cairo_matrix_init_identity (&font_matrix);
  cairo_scaled_font_get_scale_matrix (scaled_font, &ctm);

  font_options = cairo_font_options_create ();
  cairo_scaled_font_get_font_options (scaled_font, font_options);

  fallback_scaled_font = cairo_scaled_font_create (font_face,
						   &font_matrix,
						   &ctm,
						   font_options);

  cairo_font_options_destroy (font_options);

  cairo_scaled_font_extents (fallback_scaled_font, extents);

  cairo_scaled_font_destroy (fallback_scaled_font);

  cairo_scaled_font_set_user_data (scaled_font,
				   &fallback_font_face_key,
				   font_face,
				   cairo_font_face_destroy);

  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
test_scaled_font_render_glyph (cairo_scaled_font_t  *scaled_font,
			       unsigned long         glyph,
			       cairo_t              *cr,
			       cairo_text_extents_t *extents)
{
    char text[2] = "\0";

    /* XXX only works for ASCII.  need ucs4_to_utf8 :( */
    text[0] = glyph;

    cairo_set_font_face (cr,
			 cairo_scaled_font_get_user_data (scaled_font,
							  &fallback_font_face_key));
    cairo_show_text (cr, text);
    cairo_text_extents (cr, text, extents);

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
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width (cr, 2);
    cairo_stroke (cr);

    /* ink boundaries in green */
    cairo_rectangle (cr,
		     BORDER + extents.x_bearing, BORDER + font_extents.ascent + extents.y_bearing,
		     extents.width, extents.height);
    cairo_set_source_rgb (cr, 0, 1, 0);
    cairo_set_line_width (cr, 2);
    cairo_stroke (cr);

    /* text in gray */
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to (cr, BORDER, BORDER + font_extents.ascent);
    cairo_show_text (cr, text);


    /* filled version of text in light blue */
    cairo_set_source_rgb (cr, 0, 0, 1);
    cairo_move_to (cr, BORDER, BORDER + font_extents.height + BORDER + font_extents.ascent);
    cairo_text_path (cr, text);
    cairo_fill (cr);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}
