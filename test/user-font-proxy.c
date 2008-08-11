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

static const cairo_test_t test = {
    "user-font-proxy",
    "Tests a user-font using a native font in its render_glyph",
#ifndef ROTATED
    WIDTH, HEIGHT,
#else
    WIDTH, WIDTH,
#endif
    draw
};

static cairo_user_data_key_t fallback_font_key;

static cairo_status_t
test_scaled_font_init (cairo_scaled_font_t  *scaled_font,
		       cairo_t              *cr,
		       cairo_font_extents_t *extents)
{
    cairo_set_font_face (cr,
			 cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
							&fallback_font_key));

    cairo_scaled_font_set_user_data (scaled_font,
				     &fallback_font_key,
				     cairo_scaled_font_reference (cairo_get_scaled_font (cr)),
				     (cairo_destroy_func_t) cairo_scaled_font_destroy);

    cairo_font_extents (cr, extents);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
test_scaled_font_render_glyph (cairo_scaled_font_t  *scaled_font,
			       unsigned long         glyph,
			       cairo_t              *cr,
			       cairo_text_extents_t *extents)
{
    cairo_glyph_t cairo_glyph;

    cairo_glyph.index = glyph;
    cairo_glyph.x = 0;
    cairo_glyph.y = 0;

    cairo_set_font_face (cr,
			 cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
							&fallback_font_key));

    cairo_show_glyphs (cr, &cairo_glyph, 1);
    cairo_glyph_extents (cr, &cairo_glyph, 1, extents);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
test_scaled_font_text_to_glyphs (cairo_scaled_font_t   *scaled_font,
				 const char	       *utf8,
				 int		        utf8_len,
				 cairo_glyph_t	      **glyphs,
				 int		       *num_glyphs,
				 cairo_text_cluster_t **clusters,
				 int		       *num_clusters,
				 cairo_bool_t	       *backward)
{
  cairo_scaled_font_t *fallback_scaled_font;

  fallback_scaled_font = cairo_scaled_font_get_user_data (scaled_font,
							  &fallback_font_key);

  return cairo_scaled_font_text_to_glyphs (fallback_scaled_font, 0, 0,
					   utf8, utf8_len,
					   glyphs, num_glyphs,
					   clusters, num_clusters,
					   backward);
}

static cairo_font_face_t *user_font_face = NULL;

static cairo_font_face_t *
get_user_font_face (void)
{
    if (!user_font_face) {
	cairo_font_face_t *fallback_font_face;

	user_font_face = cairo_user_font_face_create ();
	cairo_user_font_face_set_init_func             (user_font_face, test_scaled_font_init);
	cairo_user_font_face_set_render_glyph_func     (user_font_face, test_scaled_font_render_glyph);
	cairo_user_font_face_set_text_to_glyphs_func   (user_font_face, test_scaled_font_text_to_glyphs);

	/* This also happens to be default font face on cairo_t, so does
	 * not make much sense here.  For demonstration only.
	 */
	fallback_font_face = cairo_toy_font_face_create ("",
							 CAIRO_FONT_SLANT_NORMAL,
							 CAIRO_FONT_WEIGHT_NORMAL);

	cairo_font_face_set_user_data (user_font_face,
				       &fallback_font_key,
				       fallback_font_face,
				       (cairo_destroy_func_t) cairo_font_face_destroy);
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

    if (user_font_face) {
        cairo_font_face_destroy (user_font_face);
	user_font_face = NULL;
    }

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}
