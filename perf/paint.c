/*
 * Copyright © 2006 Red Hat, Inc.
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

#include "cairo-perf.h"

static cairo_perf_ticks_t
do_paint (cairo_t *cr, int width, int height)
{
    cairo_perf_timer_start ();

    cairo_paint (cr);

    cairo_perf_timer_stop ();

    return cairo_perf_timer_elapsed ();
}

static void
set_source_surface (cairo_t		*cr,
		    cairo_content_t	 content,
		    int			 width,
		    int			 height)
{
    cairo_surface_t *source;
    cairo_t *cr2;

    source = cairo_surface_create_similar (cairo_get_target (cr),
					   content, width, height);

    /* Fill it with something known */
    cr2 = cairo_create (source);
    cairo_set_operator (cr2, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr2);

    cairo_set_operator (cr2, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb (cr2, 0, 0, 1);
    cairo_paint (cr2);

    cairo_set_source_rgba (cr2, 1, 0, 0, 0.5);
    cairo_new_path (cr2);
    cairo_rectangle (cr2, 0, 0, width/2.0, height/2.0);
    cairo_rectangle (cr2, width/2.0, height/2.0, width/2.0, height/2.0);
    cairo_fill (cr2);
    cairo_destroy (cr2);

    cairo_set_source_surface (cr, source, 0, 0);
    cairo_surface_destroy (source);
}

void
paint (cairo_perf_t *perf, cairo_t *cr, int width, int height)
{
    cairo_set_source_rgb (cr, 0.2, 0.6, 0.9);
    cairo_perf_run (perf, "paint_over_solid", do_paint);

    cairo_set_source_rgba (cr, 0.2, 0.6, 0.9, 0.7);
    cairo_perf_run (perf, "paint_over_solid_alpha", do_paint);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb (cr, 0.2, 0.6, 0.9);
    cairo_perf_run (perf, "paint_source_solid", do_paint);

    cairo_set_source_rgba (cr, 0.2, 0.6, 0.9, 0.7);
    cairo_perf_run (perf, "paint_source_solid_alpha", do_paint);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    set_source_surface (cr, CAIRO_CONTENT_COLOR, width, height);
    cairo_perf_run (perf, "paint_over_surf_rgb24", do_paint);

    set_source_surface (cr, CAIRO_CONTENT_COLOR_ALPHA, width, height);
    cairo_perf_run (perf, "paint_over_surf_argb32", do_paint);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    set_source_surface (cr, CAIRO_CONTENT_COLOR, width, height);
    cairo_perf_run (perf, "paint_source_surf_rgb24", do_paint);

    set_source_surface (cr, CAIRO_CONTENT_COLOR_ALPHA, width, height);
    cairo_perf_run (perf, "paint_source_surf_argb32", do_paint);
}
