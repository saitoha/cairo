/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © 2004,2007 Red Hat, Inc.
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
#include "cairo-boilerplate-quartz-private.h"

#include <cairo-quartz.h>

cairo_surface_t *
_cairo_boilerplate_quartz_create_surface (const char			 *name,
					  cairo_content_t		  content,
					  int				  width,
					  int				  height,
					  cairo_boilerplate_mode_t	  mode,
					  void				**closure)
{
    cairo_format_t format;

    switch (content) {
	case CAIRO_CONTENT_COLOR: format = CAIRO_FORMAT_RGB24; break;
	case CAIRO_CONTENT_COLOR_ALPHA: format = CAIRO_FORMAT_ARGB32; break;
	case CAIRO_CONTENT_ALPHA: format = CAIRO_FORMAT_A8; break;
	default:
	    assert (0); /* not reached */
	    return NULL;
    }

    *closure = NULL;

    return cairo_quartz_surface_create (format, width, height);
}

void
_cairo_boilerplate_quartz_cleanup (void *closure)
{
    /* nothing */
}
