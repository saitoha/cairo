/*
 * Copyright � 2002 University of Southern California
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include <stdlib.h>

#include "cairoint.h"

void
_cairo_surface_init (cairo_surface_t			*surface,
		     const cairo_surface_backend_t	*backend)
{
    surface->backend = backend;

    surface->ref_count = 1;

    _cairo_matrix_init (&surface->matrix);
    surface->filter = CAIRO_FILTER_NEAREST;
    surface->repeat = 0;
}

cairo_surface_t *
cairo_surface_create_for_image (char		*data,
				cairo_format_t	format,
				int		width,
				int		height,
				int		stride)
{
    return cairo_image_surface_create_for_data (data, format, width, height, stride);
}
slim_hidden_def(cairo_surface_create_for_image);

cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height)
{
    cairo_color_t empty;

    if (other == NULL)
	return NULL;

    _cairo_color_init (&empty);
    _cairo_color_set_rgb (&empty, 0., 0., 0.);
    _cairo_color_set_alpha (&empty, 0.);

    return _cairo_surface_create_similar_solid (other, format, width, height, &empty);
}

cairo_surface_t *
_cairo_surface_create_similar_solid (cairo_surface_t	*other,
				     cairo_format_t	format,
				     int		width,
				     int		height,
				     cairo_color_t	*color)
{
    cairo_status_t status;
    cairo_surface_t *surface = NULL;

    surface = other->backend->create_similar (other, format, width, height);
    if (surface == NULL)
	surface = cairo_image_surface_create (format, width, height);

    status = _cairo_surface_fill_rectangle (surface,
					    CAIRO_OPERATOR_SRC, color,
					    0, 0, width, height);
    if (status) {
	cairo_surface_destroy (surface);
	return NULL;
    }

    return surface;
}

void
cairo_surface_reference (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count++;
}

void
cairo_surface_destroy (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count--;
    if (surface->ref_count)
	return;

    if (surface->backend->destroy)
	surface->backend->destroy (surface);
}
slim_hidden_def(cairo_surface_destroy);

double
_cairo_surface_pixels_per_inch (cairo_surface_t *surface)
{
    return surface->backend->pixels_per_inch (surface);
}

cairo_image_surface_t *
_cairo_surface_get_image (cairo_surface_t *surface)
{
    return surface->backend->get_image (surface);
}

cairo_status_t
_cairo_surface_set_image (cairo_surface_t *surface, cairo_image_surface_t *image)
{
    return surface->backend->set_image (surface, image);
}

cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    cairo_matrix_copy (&surface->matrix, matrix);

    return surface->backend->set_matrix (surface, matrix);
}
slim_hidden_def(cairo_surface_set_matrix);

cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    return cairo_matrix_copy (matrix, &surface->matrix);
}
slim_hidden_def(cairo_surface_get_matrix);

cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    surface->filter = filter;
    return surface->backend->set_filter (surface, filter);
}

cairo_filter_t
cairo_surface_get_filter (cairo_surface_t *surface)
{
    return surface->filter;
}

/* XXX: NYI
cairo_status_t
cairo_surface_clip_rectangle (cairo_surface_t *surface,
			      int x, int y,
			      int width, int height)
{

}
*/

/* XXX: NYI
cairo_status_t
cairo_surface_clip_restore (cairo_surface_t *surface);
*/

cairo_status_t
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    surface->repeat = repeat;

    return surface->backend->set_repeat (surface, repeat);
}
slim_hidden_def(cairo_surface_set_repeat);

cairo_int_status_t
_cairo_surface_composite (cairo_operator_t	operator,
			  cairo_surface_t	*src,
			  cairo_surface_t	*mask,
			  cairo_surface_t	*dst,
			  int			src_x,
			  int			src_y,
			  int			mask_x,
			  int			mask_y,
			  int			dst_x,
			  int			dst_y,
			  unsigned int		width,
			  unsigned int		height)
{
    cairo_int_status_t status;
    cairo_image_surface_t *src_image, *mask_image, *dst_image;

    status = dst->backend->composite (operator,
				      src, mask, dst,
				      src_x, src_y,
				      mask_x, mask_y,
				      dst_x, dst_y,
				      width, height);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    src_image = _cairo_surface_get_image (src);
    if (mask)
	mask_image = _cairo_surface_get_image (mask);
    dst_image = _cairo_surface_get_image (dst);

    dst_image->base.backend->composite (operator,
					&src_image->base,
					mask ? &mask_image->base : NULL,
					dst_image,
					src_x, src_y,
					mask_x, mask_y,
					dst_x, dst_y,
					width, height);

    status = _cairo_surface_set_image (dst, dst_image);

    cairo_surface_destroy (&src_image->base);
    if (mask)
	cairo_surface_destroy (&mask_image->base);
    cairo_surface_destroy (&dst_image->base);

    return status;
}

cairo_status_t
_cairo_surface_fill_rectangle (cairo_surface_t	*surface,
			       cairo_operator_t	operator,
			       cairo_color_t	*color,
			       int		x,
			       int		y,
			       int		width,
			       int		height)
{
    cairo_rectangle_t rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    return _cairo_surface_fill_rectangles (surface, operator, color, &rect, 1);
}

cairo_status_t
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t	operator,
				const cairo_color_t	*color,
				cairo_rectangle_t	*rects,
				int			num_rects)
{
    cairo_int_status_t status;
    cairo_image_surface_t *surface_image;

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;

    status = surface->backend->fill_rectangles (surface,
						operator,
						color,
						rects, num_rects);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    surface_image = _cairo_surface_get_image (surface);

    surface_image->base.backend->fill_rectangles (surface_image,
						  operator,
						  color,
						  rects, num_rects);

    status = _cairo_surface_set_image (surface, surface_image);

    cairo_surface_destroy (&surface_image->base);

    return status;
}

cairo_status_t
_cairo_surface_composite_trapezoids (cairo_operator_t		operator,
				     cairo_surface_t		*src,
				     cairo_surface_t		*dst,
				     int			x_src,
				     int			y_src,
				     cairo_trapezoid_t		*traps,
				     int			num_traps)
{
    cairo_int_status_t status;
    cairo_image_surface_t *src_image, *dst_image;

    status = dst->backend->composite_trapezoids (operator,
						 src, dst,
						 x_src, y_src,
						 traps, num_traps);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    src_image = _cairo_surface_get_image (src);
    dst_image = _cairo_surface_get_image (dst);

    dst_image->base.backend->composite_trapezoids (operator,
						   &src_image->base,
						   dst_image,
						   x_src, y_src,
						   traps, num_traps);

    status = _cairo_surface_set_image (dst, dst_image);

    cairo_surface_destroy (&src_image->base);
    cairo_surface_destroy (&dst_image->base);

    return status;
}

cairo_status_t
_cairo_surface_copy_page (cairo_surface_t *surface)
{
    cairo_int_status_t status;

    status = surface->backend->copy_page (surface);
    /* It's fine if some backends just don't support this. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_STATUS_SUCCESS;
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_show_page (cairo_surface_t *surface)
{
    cairo_int_status_t status;

    status = surface->backend->show_page (surface);
    /* It's fine if some backends just don't support this. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_STATUS_SUCCESS;
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_set_clip_region (cairo_surface_t *surface, pixman_region16_t *region)
{
    return surface->backend->set_clip_region (surface, region);
}
