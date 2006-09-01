/*
 * Copyright © 2006 Mozilla Corporation
 * Copyright © 2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * the authors not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The authors make no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Vladimir Vukicevic <vladimir@pobox.com>
 *          Carl Worth <cworth@cworth.org>
 */

#include "cairo-perf.h"

int cairo_perf_iterations = 100;

int cairo_perf_alarm_expired = 0;

typedef struct _cairo_perf {
    const char *name;
    cairo_perf_func_t run;
    unsigned int min_size;
    unsigned int max_size;
} cairo_perf_t;

cairo_perf_t perfs[];

/* Some targets just aren't that interesting for performance testing,
 * (not least because many of these surface types use a meta-surface
 * and as such defer the "real" rendering to later, so our timing
 * loops wouldn't count the real work, just the recording by the
 * meta-surface. */
static cairo_bool_t
target_is_measurable (cairo_test_target_t *target)
{
    switch (target->expected_type) {
    case CAIRO_SURFACE_TYPE_IMAGE:
	if (strcmp (target->name, "pdf") == 0 ||
	    strcmp (target->name, "ps") == 0)
	{
	    return FALSE;
	}
	else
	{
	    return TRUE;
	}
    case CAIRO_SURFACE_TYPE_XLIB:
    case CAIRO_SURFACE_TYPE_XCB:
    case CAIRO_SURFACE_TYPE_GLITZ:
    case CAIRO_SURFACE_TYPE_QUARTZ:
    case CAIRO_SURFACE_TYPE_WIN32:
    case CAIRO_SURFACE_TYPE_BEOS:
    case CAIRO_SURFACE_TYPE_DIRECTFB:
	return TRUE;
    case CAIRO_SURFACE_TYPE_PDF:
    case CAIRO_SURFACE_TYPE_PS:
    case CAIRO_SURFACE_TYPE_SVG:
    default:
	return FALSE;
    }
}

static const char *
_content_to_string (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_COLOR:
	return "rgb";
    case CAIRO_CONTENT_ALPHA:
	return "a";
    case CAIRO_CONTENT_COLOR_ALPHA:
	return "rgba";
    default:
	return "<unknown_content>";
    }
}

typedef struct _stats
{
    double mean;
    double std_dev;
} stats_t;

static int
compare_doubles (const void *_a, const void *_b)
{
    const double *a = _a;
    const double *b = _b;

    return *a - *b;
}

static void
_compute_stats (double *values, int num_values, stats_t *stats)
{
    int i;
    double sum, delta;
    int chop = num_values / 5;

    qsort (values, num_values, sizeof (double), compare_doubles);

    num_values -= 2 * chop;

    sum = 0.0;
    for (i = chop; i < chop + num_values; i++)
	sum += values[i];

    stats->mean = sum / num_values;

    sum = 0.0;
    for (i = chop; i < chop + num_values; i++) {
	delta = values[i] - stats->mean;
	sum += delta * delta;
    }

    /* Let's use a std. deviation normalized to the mean for easier
     * comparison. */
    stats->std_dev = sqrt(sum / num_values) / stats->mean;
}

int
main (int argc, char *argv[])
{
    int i, j, k;
    cairo_test_target_t *target;
    cairo_perf_t *perf;
    cairo_surface_t *surface;
    cairo_t *cr;
    unsigned int size;
    double *rates;
    stats_t stats;

    if (getenv("CAIRO_PERF_ITERATIONS"))
	cairo_perf_iterations = strtol(getenv("CAIRO_PERF_ITERATIONS"), NULL, 0);

    rates = xmalloc (cairo_perf_iterations * sizeof (double));

    for (i = 0; targets[i].name; i++) {
	target = &targets[i];
	if (! target_is_measurable (target))
	    continue;
	for (j = 0; perfs[j].name; j++) {
	    perf = &perfs[j];
	    for (size = perf->min_size; size <= perf->max_size; size *= 2) {
		surface = (target->create_target_surface) (perf->name,
							   target->content,
							   size, size,
							   &target->closure);
		cr = cairo_create (surface);
		for (k =0; k < cairo_perf_iterations; k++)
		    rates[k] = perf->run (cr, size, size);
		_compute_stats (rates, cairo_perf_iterations, &stats);
		if (i==0 && j==0 && size == perf->min_size)
		    printf ("backend-content\ttest-size\trate\tstd dev.\titerations\n");
		printf ("%s-%s\t%s-%d\t%g\t%g%%\t%d\n",
			target->name, _content_to_string (target->content),
			perf->name, size,
			stats.mean, stats.std_dev * 100.0, cairo_perf_iterations);
	    }
	}
    }

    return 0;
}

cairo_perf_t perfs[] = {
    { "paint", paint, 64, 512 },
    { "paint_alpha", paint_alpha, 64, 512 },
    { NULL }
};
