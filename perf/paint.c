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

void
paint_setup (cairo_t *cr, int width, int height)
{
    cairo_set_source_rgba (cr, 1.0, 0.2, 0.6, 0.5);
}

void
paint_alpha_setup (cairo_t *cr, int width, int height)
{
    cairo_set_source_rgb (cr, 0.2, 0.6, 0.9);
}

void
paint (cairo_t *cr, int width, int height)
{
    cairo_perf_timer_t timer;

    CAIRO_PERF_LOOP_INIT (timer);
    {
	cairo_paint (cr);
    }
    CAIRO_PERF_LOOP_FINI (timer);

    printf ("Rate: %g\n", CAIRO_PERF_LOOP_RATE (timer));
}
