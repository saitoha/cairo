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

#define USE_WINAPI

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cairo-perf.h"

/* timers */

typedef struct _cairo_perf_timer
{
    LARGE_INTEGER start;
    LARGE_INTEGER stop;
} cairo_perf_timer_t;

static cairo_perf_timer_t timer;

void
cairo_perf_timer_start (void) {
    QueryPerformanceCounter(&timer.start);
}

void
cairo_perf_timer_stop (void) {
    QueryPerformanceCounter(&timer.stop);
}

double
cairo_perf_timer_elapsed (void) {
    double d;
    LARGE_INTEGER freq;

    QueryPerformanceFrequency(&freq);

    d = (timer.stop.QuadPart - timer.start.QuadPart) / (double) freq.QuadPart;
    return d;
}

/* yield */

void
cairo_perf_yield (void) {
    SleepEx(0, TRUE);
}
