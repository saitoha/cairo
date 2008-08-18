/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © 2004,2006 Red Hat, Inc.
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
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include "cairo-boilerplate.h"
#include "cairo-boilerplate-scaled-font.h"

#if CAIRO_HAS_BEOS_SURFACE
#include "cairo-boilerplate-beos-private.h"
#endif
#if CAIRO_HAS_DIRECTFB_SURFACE
#include "cairo-boilerplate-directfb-private.h"
#endif
#if CAIRO_HAS_GLITZ_SURFACE
#include "cairo-boilerplate-glitz-private.h"
#endif
#if CAIRO_HAS_PDF_SURFACE
#include "cairo-boilerplate-pdf-private.h"
#endif
#if CAIRO_HAS_PS_SURFACE
#include "cairo-boilerplate-ps-private.h"
#endif
#if CAIRO_HAS_QUARTZ_SURFACE
#include "cairo-boilerplate-quartz-private.h"
#endif
#if CAIRO_HAS_SVG_SURFACE
#include "cairo-boilerplate-svg-private.h"
#endif
#ifdef CAIRO_HAS_TEST_SURFACES
#include "cairo-boilerplate-test-surfaces-private.h"
#endif
#if CAIRO_HAS_WIN32_SURFACE
#include "cairo-boilerplate-win32-private.h"
#endif
#if CAIRO_HAS_XCB_SURFACE
#include "cairo-boilerplate-xcb-private.h"
#endif
#if CAIRO_HAS_XLIB_SURFACE
#include "cairo-boilerplate-xlib-private.h"
#endif

#include <cairo-types-private.h>
#include <cairo-scaled-font-private.h>

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

cairo_content_t
cairo_boilerplate_content (cairo_content_t content)
{
    if (content == CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED)
	content = CAIRO_CONTENT_COLOR_ALPHA;

    return content;
}

const char *
cairo_boilerplate_content_name (cairo_content_t content)
{
    /* For the purpose of the content name, we don't distinguish the
     * flattened content value.
     */
    switch (cairo_boilerplate_content (content)) {
    case CAIRO_CONTENT_COLOR:
	return "rgb24";
    case CAIRO_CONTENT_COLOR_ALPHA:
	return "argb32";
    case CAIRO_CONTENT_ALPHA:
    default:
	assert (0); /* not reached */
	return "---";
    }
}

cairo_format_t
cairo_boilerplate_format_from_content (cairo_content_t content)
{
    cairo_format_t format;

    switch (content) {
	case CAIRO_CONTENT_COLOR: format = CAIRO_FORMAT_RGB24; break;
	case CAIRO_CONTENT_COLOR_ALPHA: format = CAIRO_FORMAT_ARGB32; break;
	case CAIRO_CONTENT_ALPHA: format = CAIRO_FORMAT_A8; break;
	default:
	    assert (0); /* not reached */
	    format = (cairo_format_t) -1;
	    break;
    }

    return format;
}

static cairo_surface_t *
_cairo_boilerplate_image_create_surface (const char			 *name,
					 cairo_content_t		  content,
					 int				  width,
					 int				  height,
					 int				  max_width,
					 int				  max_height,
					 cairo_boilerplate_mode_t	  mode,
					 int                              id,
					 void				**closure)
{
    cairo_format_t format;

    *closure = NULL;

    if (content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = CAIRO_FORMAT_ARGB32;
    } else if (content == CAIRO_CONTENT_COLOR) {
	format = CAIRO_FORMAT_RGB24;
    } else {
	assert (0); /* not reached */
	return NULL;
    }

    return cairo_image_surface_create (format, width, height);
}

cairo_surface_t *
_cairo_boilerplate_get_image_surface (cairo_surface_t *src,
				      int width,
				      int height)
{
    cairo_surface_t *surface;
    cairo_t *cr;

#if 0
    if (cairo_surface_get_type (src) == CAIRO_SURFACE_TYPE_IMAGE) {
	int ww = cairo_image_surface_get_width (src);
	int hh = cairo_image_surface_get_height (src);
	if (width == ww && hh == height) {
	    return cairo_surface_reference (src);
	} else {
	    cairo_format_t format = cairo_image_surface_get_format (src);
	    unsigned char *data = cairo_image_surface_get_data (src);
	    int stride = cairo_image_surface_get_stride (src);

	    data += stride * (hh - height) + 4 * (ww - width);
	    return cairo_image_surface_create_for_data (data,
							format,
							width,
							height,
							stride);
	}
    }
#endif

    /* extract sub-surface */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_set_source_surface (cr, src, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);

    return surface;
}

cairo_surface_t *
cairo_boilerplate_get_image_surface_from_png (const char *filename,
					      int width,
					      int height,
					      cairo_bool_t flatten)
{
    cairo_surface_t *surface;

    surface = cairo_image_surface_create_from_png (filename);

    if (flatten) {
	cairo_t *cr;
	cairo_surface_t *flattened;

	flattened = cairo_image_surface_create (cairo_image_surface_get_format (surface),
						width,
						height);
	cr = cairo_create (flattened);
	cairo_surface_destroy (flattened);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	cairo_set_source_surface (cr, surface,
				  width - cairo_image_surface_get_width (surface),
				  height - cairo_image_surface_get_height (surface));
	cairo_paint (cr);

	cairo_surface_destroy (surface);
	surface = cairo_surface_reference (cairo_get_target (cr));
	cairo_destroy (cr);
    } else if (cairo_image_surface_get_width (surface) != width ||
	       cairo_image_surface_get_height (surface) != height)
    {
	cairo_t *cr;
	cairo_surface_t *sub;

	sub = cairo_image_surface_create (cairo_image_surface_get_format (surface),
					  width,
					  height);
	cr = cairo_create (sub);
	cairo_surface_destroy (sub);

	cairo_set_source_surface (cr, surface,
				  width - cairo_image_surface_get_width (surface),
				  height - cairo_image_surface_get_height (surface));
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (cr);

	cairo_surface_destroy (surface);
	surface = cairo_surface_reference (cairo_get_target (cr));
	cairo_destroy (cr);
    }

    return surface;
}

static cairo_boilerplate_target_t targets[] =
{
    /* I'm uncompromising about leaving the image backend as 0
     * for tolerance. There shouldn't ever be anything that is out of
     * our control here. */
    { "image", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_image_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
    { "image", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_image_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
#ifdef CAIRO_HAS_TEST_SURFACES
    { "test-fallback", CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
      CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_test_fallback_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
    { "test-fallback", CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
      CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_test_fallback_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
    { "test-meta", CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
      CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_test_meta_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
    { "test-meta", CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
      CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_test_meta_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
    { "test-paginated", CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,
      CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_test_paginated_create_surface,
      _cairo_boilerplate_test_paginated_get_image_surface,
      _cairo_boilerplate_test_paginated_surface_write_to_png,
      _cairo_boilerplate_test_paginated_cleanup },
    { "test-paginated", CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,
      CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_test_paginated_create_surface,
      _cairo_boilerplate_test_paginated_get_image_surface,
      _cairo_boilerplate_test_paginated_surface_write_to_png,
      _cairo_boilerplate_test_paginated_cleanup },
#endif
#ifdef CAIRO_HAS_GLITZ_SURFACE
#if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
    { "glitz-glx", CAIRO_SURFACE_TYPE_GLITZ,CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_glitz_glx_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_glitz_glx_cleanup },
    { "glitz-glx", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_glitz_glx_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_glitz_glx_cleanup },
#endif
#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
    { "glitz-agl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_glitz_agl_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_glitz_agl_cleanup },
    { "glitz-agl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_glitz_agl_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_glitz_agl_cleanup },
#endif
#if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
    { "glitz-wgl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_glitz_wgl_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_glitz_wgl_cleanup },
    { "glitz-wgl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_glitz_wgl_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_glitz_wgl_cleanup },
#endif
#endif /* CAIRO_HAS_GLITZ_SURFACE */
#if CAIRO_HAS_QUARTZ_SURFACE
    { "quartz", CAIRO_SURFACE_TYPE_QUARTZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_quartz_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_quartz_cleanup },
    { "quartz", CAIRO_SURFACE_TYPE_QUARTZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_quartz_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_quartz_cleanup },
#endif
#if CAIRO_HAS_WIN32_SURFACE
    { "win32", CAIRO_SURFACE_TYPE_WIN32, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_win32_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
    /* Testing the win32 surface isn't interesting, since for
     * ARGB images it just chains to the image backend
     */
    { "win32", CAIRO_SURFACE_TYPE_WIN32, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_win32_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png },
#if CAIRO_CAN_TEST_WIN32_PRINTING_SURFACE
    { "win32-printing", CAIRO_SURFACE_TYPE_WIN32_PRINTING,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED, 0,
      _cairo_boilerplate_win32_printing_create_surface,
      _cairo_boilerplate_win32_printing_get_image_surface,
      _cairo_boilerplate_win32_printing_surface_write_to_png,
      _cairo_boilerplate_win32_printing_cleanup,
      NULL, TRUE },
    { "win32-printing", CAIRO_INTERNAL_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_win32_printing_create_surface,
      _cairo_boilerplate_win32_printing_get_image_surface,
      _cairo_boilerplate_win32_printing_surface_write_to_png,
      _cairo_boilerplate_win32_printing_cleanup,
      NULL, TRUE },
#endif
#endif
#if CAIRO_HAS_XCB_SURFACE
    /* Acceleration architectures may make the results differ by a
     * bit, so we set the error tolerance to 1. */
    { "xcb", CAIRO_SURFACE_TYPE_XCB, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_xcb_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xcb_cleanup,
      _cairo_boilerplate_xcb_synchronize},
#endif
#if CAIRO_HAS_XLIB_XRENDER_SURFACE
    /* Acceleration architectures may make the results differ by a
     * bit, so we set the error tolerance to 1. */
    { "xlib", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_xlib_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xlib_cleanup,
      _cairo_boilerplate_xlib_synchronize},
    { "xlib", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_xlib_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xlib_cleanup,
      _cairo_boilerplate_xlib_synchronize},
#endif
#if CAIRO_HAS_XLIB_SURFACE
    /* This is a fallback surface which uses xlib fallbacks instead of
     * the Render extension. */
    { "xlib-fallback", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_xlib_fallback_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xlib_cleanup,
      _cairo_boilerplate_xlib_synchronize},
#endif
#if CAIRO_HAS_PS_SURFACE && CAIRO_CAN_TEST_PS_SURFACE
    { "ps", CAIRO_SURFACE_TYPE_PS,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED, 0,
      _cairo_boilerplate_ps_create_surface,
      _cairo_boilerplate_ps_get_image_surface,
      _cairo_boilerplate_ps_surface_write_to_png,
      _cairo_boilerplate_ps_cleanup,
      NULL, TRUE },
    { "ps", CAIRO_INTERNAL_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_ps_create_surface,
      _cairo_boilerplate_ps_get_image_surface,
      _cairo_boilerplate_ps_surface_write_to_png,
      _cairo_boilerplate_ps_cleanup,
      NULL, TRUE },
#endif
#if CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE
    { "pdf", CAIRO_SURFACE_TYPE_PDF,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED, 0,
      _cairo_boilerplate_pdf_create_surface,
      _cairo_boilerplate_pdf_get_image_surface,
      _cairo_boilerplate_pdf_surface_write_to_png,
      _cairo_boilerplate_pdf_cleanup,
      NULL, TRUE },
    { "pdf", CAIRO_INTERNAL_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_pdf_create_surface,
      _cairo_boilerplate_pdf_get_image_surface,
      _cairo_boilerplate_pdf_surface_write_to_png,
      _cairo_boilerplate_pdf_cleanup,
      NULL, TRUE },
#endif
#if CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE
    /* It seems we should be able to round-trip SVG content perfrectly
     * through librsvg and cairo, but for some mysterious reason, some
     * systems get an error of 1 for some pixels on some of the text
     * tests. XXX: I'd still like to chase these down at some point.
     * For now just set the svg error tolerance to 1. */
    { "svg", CAIRO_SURFACE_TYPE_SVG, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_svg_create_surface,
      _cairo_boilerplate_svg_get_image_surface,
      _cairo_boilerplate_svg_surface_write_to_png,
      _cairo_boilerplate_svg_cleanup,
      NULL, TRUE },
    { "svg", CAIRO_INTERNAL_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_svg_create_surface,
      _cairo_boilerplate_svg_get_image_surface,
      _cairo_boilerplate_svg_surface_write_to_png,
      _cairo_boilerplate_svg_cleanup,
      NULL, TRUE },
#endif
#if CAIRO_HAS_BEOS_SURFACE
    /* BeOS sometimes produces a slightly different image. Perhaps this
     * is related to the fact that it doesn't use premultiplied alpha...
     * Just ignore the small difference. */
    { "beos", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_beos_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_beos_cleanup},
    { "beos-bitmap", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_beos_create_surface_for_bitmap,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_beos_cleanup_bitmap},
    { "beos-bitmap", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_beos_create_surface_for_bitmap,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_beos_cleanup_bitmap},
#endif


#if CAIRO_HAS_DIRECTFB_SURFACE
    { "directfb", CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_directfb_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_directfb_cleanup},
    { "directfb-bitmap", CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_directfb_create_surface,
      _cairo_boilerplate_get_image_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_directfb_cleanup},
#endif
};

cairo_boilerplate_target_t **
cairo_boilerplate_get_targets (int *pnum_targets, cairo_bool_t *plimited_targets)
{
    size_t i, num_targets;
    cairo_bool_t limited_targets = FALSE;
    const char *tname;
    cairo_boilerplate_target_t **targets_to_test;

    if ((tname = getenv ("CAIRO_TEST_TARGET")) != NULL && *tname) {
	/* check the list of targets specified by the user */
	limited_targets = TRUE;

	num_targets = 0;
	targets_to_test = NULL;

	while (*tname) {
	    int found = 0;
	    const char *end = strpbrk (tname, " \t\r\n;:,");
	    if (!end)
	        end = tname + strlen (tname);

	    if (end == tname) {
		tname = end + 1;
		continue;
	    }

	    for (i = 0; i < sizeof (targets) / sizeof (targets[0]); i++) {
		if (0 == strncmp (targets[i].name, tname, end - tname) &&
		    !isalnum (targets[i].name[end - tname])) {
		    /* realloc isn't exactly the best thing here, but meh. */
		    targets_to_test = xrealloc (targets_to_test, sizeof(cairo_boilerplate_target_t *) * (num_targets+1));
		    targets_to_test[num_targets++] = &targets[i];
		    found = 1;
		}
	    }

	    if (!found) {
		fprintf (stderr, "Cannot find target '%.*s'\n", (int)(end - tname), tname);
		exit(-1);
	    }

	    if (*end)
	      end++;
	    tname = end;
	}
    } else {
	/* check all compiled in targets */
	num_targets = sizeof (targets) / sizeof (targets[0]);
	targets_to_test = xmalloc (sizeof(cairo_boilerplate_target_t*) * num_targets);
	for (i = 0; i < num_targets; i++) {
	    targets_to_test[i] = &targets[i];
	}
    }

    /* exclude targets as specified by the user */
    if ((tname = getenv ("CAIRO_TEST_TARGET_EXCLUDE")) != NULL && *tname) {
	limited_targets = TRUE;

	while (*tname) {
	    int j;
	    const char *end = strpbrk (tname, " \t\r\n;:,");
	    if (!end)
	        end = tname + strlen (tname);

	    if (end == tname) {
		tname = end + 1;
		continue;
	    }

	    for (i = j = 0; i < num_targets; i++) {
		if (strncmp (targets_to_test[i]->name, tname, end - tname) ||
		    isalnum (targets_to_test[i]->name[end - tname]))
		{
		    targets_to_test[j++] = targets_to_test[i];
		}
	    }
	    num_targets = j;

	    if (*end)
	      end++;
	    tname = end;
	}
    }

    if (pnum_targets)
	*pnum_targets = num_targets;

    if (plimited_targets)
	*plimited_targets = limited_targets;

    return targets_to_test;
}

void
cairo_boilerplate_free_targets (cairo_boilerplate_target_t **targets)
{
    free (targets);
}

cairo_surface_t *
cairo_boilerplate_surface_create_in_error (cairo_status_t status)
{
    cairo_surface_t *surface = NULL;

    do {
	cairo_surface_t *intermediate;
	cairo_t *cr;
	cairo_path_t path;

	intermediate = cairo_image_surface_create (CAIRO_FORMAT_A8, 0, 0);
	cr = cairo_create (intermediate);
	cairo_surface_destroy (intermediate);

	path.status = status;
	cairo_append_path (cr, &path);

	cairo_surface_destroy (surface);
	surface = cairo_get_target (cr);
	cairo_destroy (cr);
    } while (cairo_surface_status (surface) != status);

    return surface;
}

void
cairo_boilerplate_scaled_font_set_max_glyphs_cached (cairo_scaled_font_t *scaled_font,
						     int max_glyphs)
{
    if (cairo_scaled_font_status (scaled_font))
	return;

    scaled_font->glyphs->max_size = max_glyphs;
}
