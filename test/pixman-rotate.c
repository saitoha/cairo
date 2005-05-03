#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include "cairo-test.h"

#define WIDTH	32
#define HEIGHT	WIDTH

#define IMAGE_WIDTH	(3 * WIDTH)
#define IMAGE_HEIGHT	IMAGE_WIDTH

cairo_test_t test = {
    "pixman-rotate",
    "Exposes pixman off-by-one error when rotating",
    IMAGE_WIDTH, IMAGE_HEIGHT
};

/* Draw the word cairo at NUM_TEXT different angles */
static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_surface_t *target, *stamp;

    target = cairo_get_target_surface (cr);
    cairo_surface_reference (target);

    stamp = cairo_surface_create_similar (target, CAIRO_FORMAT_ARGB32,
					  WIDTH, HEIGHT);
    cairo_set_target_surface (cr, stamp);
    cairo_new_path (cr);
    cairo_rectangle (cr, WIDTH / 4, HEIGHT / 4, WIDTH / 2, HEIGHT / 2);
    cairo_set_source_rgba (cr, 1, 0, 0, 0.8);
    cairo_fill (cr);

    cairo_rectangle (cr, 0, 0, WIDTH, HEIGHT);
    cairo_set_line_width (cr, 2);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_stroke (cr);

    cairo_set_target_surface (cr, target);

    /* Draw a translucent rectangle for reference where the rotated
     * image should be. */
    cairo_new_path (cr);
    cairo_rectangle (cr, WIDTH, HEIGHT, WIDTH, HEIGHT);
    cairo_set_source_rgba (cr, 1, 1, 0, 0.3);
    cairo_fill (cr);

#if 1 /* Set to 0 to generate reference image */
    cairo_translate (cr, 2 * WIDTH, 2 * HEIGHT);
    cairo_rotate (cr, M_PI);
#else
    cairo_translate (cr, WIDTH, HEIGHT);
#endif

    cairo_set_source_surface (cr, stamp, 0, 0);
    cairo_paint (cr);

    cairo_show_page (cr);

    cairo_surface_destroy (stamp);
    cairo_surface_destroy (target);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test_expect_failure (&test, draw,
				      "known off-by-one bug when rotating a pixman image");
}
