/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2009,2010,2011 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

/* The purpose of this file/surface is to simply translate a pattern
 * to a pixman_image_t and thence to feed it back to the general
 * compositor interface.
 */

#include "cairoint.h"

#include "cairo-image-surface-private.h"

#include "cairo-compositor-private.h"
#include "cairo-error-private.h"
#include "cairo-pattern-private.h"
#include "cairo-paginated-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-surface-observer-private.h"
#include "cairo-surface-snapshot-private.h"
#include "cairo-surface-subsurface-private.h"

#define PIXMAN_MAX_INT ((pixman_fixed_1 >> 1) - pixman_fixed_e) /* need to ensure deltas also fit */

#if CAIRO_NO_MUTEX
#define PIXMAN_HAS_ATOMIC_OPS 1
#endif

#if PIXMAN_HAS_ATOMIC_OPS
static pixman_image_t *__pixman_transparent_image;
static pixman_image_t *__pixman_black_image;
static pixman_image_t *__pixman_white_image;

static pixman_image_t *
_pixman_transparent_image (void)
{
    pixman_image_t *image;

    image = __pixman_transparent_image;
    if (unlikely (image == NULL)) {
	pixman_color_t color;

	color.red   = 0x00;
	color.green = 0x00;
	color.blue  = 0x00;
	color.alpha = 0x00;

	image = pixman_image_create_solid_fill (&color);
	if (unlikely (image == NULL))
	    return NULL;

	if (_cairo_atomic_ptr_cmpxchg (&__pixman_transparent_image,
				       NULL, image))
	{
	    pixman_image_ref (image);
	}
    } else {
	pixman_image_ref (image);
    }

    return image;
}

static pixman_image_t *
_pixman_black_image (void)
{
    pixman_image_t *image;

    image = __pixman_black_image;
    if (unlikely (image == NULL)) {
	pixman_color_t color;

	color.red   = 0x00;
	color.green = 0x00;
	color.blue  = 0x00;
	color.alpha = 0xffff;

	image = pixman_image_create_solid_fill (&color);
	if (unlikely (image == NULL))
	    return NULL;

	if (_cairo_atomic_ptr_cmpxchg (&__pixman_black_image,
				       NULL, image))
	{
	    pixman_image_ref (image);
	}
    } else {
	pixman_image_ref (image);
    }

    return image;
}

static pixman_image_t *
_pixman_white_image (void)
{
    pixman_image_t *image;

    image = __pixman_white_image;
    if (unlikely (image == NULL)) {
	pixman_color_t color;

	color.red   = 0xffff;
	color.green = 0xffff;
	color.blue  = 0xffff;
	color.alpha = 0xffff;

	image = pixman_image_create_solid_fill (&color);
	if (unlikely (image == NULL))
	    return NULL;

	if (_cairo_atomic_ptr_cmpxchg (&__pixman_white_image,
				       NULL, image))
	{
	    pixman_image_ref (image);
	}
    } else {
	pixman_image_ref (image);
    }

    return image;
}

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static struct {
    cairo_color_t color;
    pixman_image_t *image;
} cache[16];
static int n_cached;

#else  /* !PIXMAN_HAS_ATOMIC_OPS */
static pixman_image_t *
_pixman_transparent_image (void)
{
    return _pixman_image_for_color (CAIRO_COLOR_TRANSPARENT);
}

static pixman_image_t *
_pixman_black_image (void)
{
    return _pixman_image_for_color (CAIRO_COLOR_BLACK);
}

static pixman_image_t *
_pixman_white_image (void)
{
    return _pixman_image_for_color (CAIRO_COLOR_WHITE);
}
#endif /* !PIXMAN_HAS_ATOMIC_OPS */


pixman_image_t *
_pixman_image_for_color (const cairo_color_t *cairo_color)
{
    pixman_color_t color;
    pixman_image_t *image;

#if PIXMAN_HAS_ATOMIC_OPS
    int i;

    if (CAIRO_COLOR_IS_CLEAR (cairo_color))
	return _pixman_transparent_image ();

    if (CAIRO_COLOR_IS_OPAQUE (cairo_color)) {
	if (cairo_color->red_short <= 0x00ff &&
	    cairo_color->green_short <= 0x00ff &&
	    cairo_color->blue_short <= 0x00ff)
	{
	    return _pixman_black_image ();
	}

	if (cairo_color->red_short >= 0xff00 &&
	    cairo_color->green_short >= 0xff00 &&
	    cairo_color->blue_short >= 0xff00)
	{
	    return _pixman_white_image ();
	}
    }

    CAIRO_MUTEX_LOCK (_cairo_image_solid_cache_mutex);
    for (i = 0; i < n_cached; i++) {
	if (_cairo_color_equal (&cache[i].color, cairo_color)) {
	    image = pixman_image_ref (cache[i].image);
	    goto UNLOCK;
	}
    }
#endif

    color.red   = cairo_color->red_short;
    color.green = cairo_color->green_short;
    color.blue  = cairo_color->blue_short;
    color.alpha = cairo_color->alpha_short;

    image = pixman_image_create_solid_fill (&color);
#if PIXMAN_HAS_ATOMIC_OPS
    if (image == NULL)
	goto UNLOCK;

    if (n_cached < ARRAY_LENGTH (cache)) {
	i = n_cached++;
    } else {
	i = hars_petruska_f54_1_random () % ARRAY_LENGTH (cache);
	pixman_image_unref (cache[i].image);
    }
    cache[i].image = pixman_image_ref (image);
    cache[i].color = *cairo_color;

UNLOCK:
    CAIRO_MUTEX_UNLOCK (_cairo_image_solid_cache_mutex);
#endif
    return image;
}


void
_cairo_image_reset_static_data (void)
{
#if PIXMAN_HAS_ATOMIC_OPS
    while (n_cached)
	pixman_image_unref (cache[--n_cached].image);

    if (__pixman_transparent_image) {
	pixman_image_unref (__pixman_transparent_image);
	__pixman_transparent_image = NULL;
    }

    if (__pixman_black_image) {
	pixman_image_unref (__pixman_black_image);
	__pixman_black_image = NULL;
    }

    if (__pixman_white_image) {
	pixman_image_unref (__pixman_white_image);
	__pixman_white_image = NULL;
    }
#endif
}

static pixman_image_t *
_pixman_image_for_gradient (const cairo_gradient_pattern_t *pattern,
			    const cairo_rectangle_int_t *extents,
			    int *ix, int *iy)
{
    pixman_image_t	  *pixman_image;
    pixman_gradient_stop_t pixman_stops_static[2];
    pixman_gradient_stop_t *pixman_stops = pixman_stops_static;
    pixman_transform_t      pixman_transform;
    cairo_matrix_t matrix;
    cairo_circle_double_t extremes[2];
    pixman_point_fixed_t p1, p2;
    unsigned int i;
    cairo_int_status_t status;

    if (pattern->n_stops > ARRAY_LENGTH(pixman_stops_static)) {
	pixman_stops = _cairo_malloc_ab (pattern->n_stops,
					 sizeof(pixman_gradient_stop_t));
	if (unlikely (pixman_stops == NULL))
	    return NULL;
    }

    for (i = 0; i < pattern->n_stops; i++) {
	pixman_stops[i].x = _cairo_fixed_16_16_from_double (pattern->stops[i].offset);
	pixman_stops[i].color.red   = pattern->stops[i].color.red_short;
	pixman_stops[i].color.green = pattern->stops[i].color.green_short;
	pixman_stops[i].color.blue  = pattern->stops[i].color.blue_short;
	pixman_stops[i].color.alpha = pattern->stops[i].color.alpha_short;
    }

    _cairo_gradient_pattern_fit_to_range (pattern, PIXMAN_MAX_INT >> 1, &matrix, extremes);

    p1.x = _cairo_fixed_16_16_from_double (extremes[0].center.x);
    p1.y = _cairo_fixed_16_16_from_double (extremes[0].center.y);
    p2.x = _cairo_fixed_16_16_from_double (extremes[1].center.x);
    p2.y = _cairo_fixed_16_16_from_double (extremes[1].center.y);

    if (pattern->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	pixman_image = pixman_image_create_linear_gradient (&p1, &p2,
							    pixman_stops,
							    pattern->n_stops);
    } else {
	pixman_fixed_t r1, r2;

	r1   = _cairo_fixed_16_16_from_double (extremes[0].radius);
	r2   = _cairo_fixed_16_16_from_double (extremes[1].radius);

	pixman_image = pixman_image_create_radial_gradient (&p1, &p2, r1, r2,
							    pixman_stops,
							    pattern->n_stops);
    }

    if (pixman_stops != pixman_stops_static)
	free (pixman_stops);

    if (unlikely (pixman_image == NULL))
	return NULL;

    *ix = *iy = 0;
    status = _cairo_matrix_to_pixman_matrix_offset (&matrix, pattern->base.filter,
						    extents->x + extents->width/2.,
						    extents->y + extents->height/2.,
						    &pixman_transform, ix, iy);
    if (status != CAIRO_INT_STATUS_NOTHING_TO_DO) {
	if (unlikely (status != CAIRO_INT_STATUS_SUCCESS) ||
	    ! pixman_image_set_transform (pixman_image, &pixman_transform))
	{
	    pixman_image_unref (pixman_image);
	    return NULL;
	}
    }

    {
	pixman_repeat_t pixman_repeat;

	switch (pattern->base.extend) {
	default:
	case CAIRO_EXTEND_NONE:
	    pixman_repeat = PIXMAN_REPEAT_NONE;
	    break;
	case CAIRO_EXTEND_REPEAT:
	    pixman_repeat = PIXMAN_REPEAT_NORMAL;
	    break;
	case CAIRO_EXTEND_REFLECT:
	    pixman_repeat = PIXMAN_REPEAT_REFLECT;
	    break;
	case CAIRO_EXTEND_PAD:
	    pixman_repeat = PIXMAN_REPEAT_PAD;
	    break;
	}

	pixman_image_set_repeat (pixman_image, pixman_repeat);
    }

    return pixman_image;
}

static pixman_image_t *
_pixman_image_for_mesh (const cairo_mesh_pattern_t *pattern,
			const cairo_rectangle_int_t *extents,
			int *tx, int *ty)
{
    pixman_image_t *image;
    int width, height;

    *tx = -extents->x;
    *ty = -extents->y;
    width = extents->width;
    height = extents->height;

    image = pixman_image_create_bits (PIXMAN_a8r8g8b8, width, height, NULL, 0);
    if (unlikely (image == NULL))
	return NULL;

    _cairo_mesh_pattern_rasterize (pattern,
				   pixman_image_get_data (image),
				   width, height,
				   pixman_image_get_stride (image),
				   *tx, *ty);
    return image;
}

struct acquire_source_cleanup {
    cairo_surface_t *surface;
    cairo_image_surface_t *image;
    void *image_extra;
};

static void
_acquire_source_cleanup (pixman_image_t *pixman_image,
			 void *closure)
{
    struct acquire_source_cleanup *data = closure;

    _cairo_surface_release_source_image (data->surface,
					 data->image,
					 data->image_extra);
    free (data);
}

static uint16_t
expand_channel (uint16_t v, uint32_t bits)
{
    int offset = 16 - bits;
    while (offset > 0) {
	v |= v >> bits;
	offset -= bits;
	bits += bits;
    }
    return v;
}

static pixman_image_t *
_pixel_to_solid (cairo_image_surface_t *image, int x, int y)
{
    uint32_t pixel;
    pixman_color_t color;

    switch (image->format) {
    default:
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return NULL;

    case CAIRO_FORMAT_A1:
	pixel = *(uint8_t *) (image->data + y * image->stride + x/8);
	return pixel & (1 << (x&7)) ? _pixman_black_image () : _pixman_transparent_image ();

    case CAIRO_FORMAT_A8:
	color.alpha = *(uint8_t *) (image->data + y * image->stride + x);
	color.alpha |= color.alpha << 8;
	if (color.alpha == 0)
	    return _pixman_transparent_image ();
	if (color.alpha == 0xffff)
	    return _pixman_black_image ();

	color.red = color.green = color.blue = 0;
	return pixman_image_create_solid_fill (&color);

    case CAIRO_FORMAT_RGB16_565:
	pixel = *(uint16_t *) (image->data + y * image->stride + 2 * x);
	if (pixel == 0)
	    return _pixman_black_image ();
	if (pixel == 0xffff)
	    return _pixman_white_image ();

	color.alpha = 0xffff;
	color.red = expand_channel ((pixel >> 11 & 0x1f) << 11, 5);
	color.green = expand_channel ((pixel >> 5 & 0x3f) << 10, 6);
	color.blue = expand_channel ((pixel & 0x1f) << 11, 5);
	return pixman_image_create_solid_fill (&color);

    case CAIRO_FORMAT_RGB30:
	pixel = *(uint32_t *) (image->data + y * image->stride + 4 * x);
	pixel &= 0x3fffffff; /* ignore alpha bits */
	if (pixel == 0)
	    return _pixman_black_image ();
	if (pixel == 0x3fffffff)
	    return _pixman_white_image ();

	/* convert 10bpc to 16bpc */
	color.alpha = 0xffff;
	color.red = expand_channel((pixel >> 20) & 0x3fff, 10);
	color.green = expand_channel((pixel >> 10) & 0x3fff, 10);
	color.blue = expand_channel(pixel & 0x3fff, 10);
	return pixman_image_create_solid_fill (&color);

    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	pixel = *(uint32_t *) (image->data + y * image->stride + 4 * x);
	color.alpha = image->format == CAIRO_FORMAT_ARGB32 ? (pixel >> 24) | (pixel >> 16 & 0xff00) : 0xffff;
	if (color.alpha == 0)
	    return _pixman_transparent_image ();
	if (pixel == 0xffffffff)
	    return _pixman_white_image ();
	if (color.alpha == 0xffff && (pixel & 0xffffff) == 0)
	    return _pixman_black_image ();

	color.red = (pixel >> 16 & 0xff) | (pixel >> 8 & 0xff00);
	color.green = (pixel >> 8 & 0xff) | (pixel & 0xff00);
	color.blue = (pixel & 0xff) | (pixel << 8 & 0xff00);
	return pixman_image_create_solid_fill (&color);
    }
}

static cairo_bool_t
_pixman_image_set_properties (pixman_image_t *pixman_image,
			      const cairo_pattern_t *pattern,
			      const cairo_rectangle_int_t *extents,
			      int *ix,int *iy)
{
    pixman_transform_t pixman_transform;
    cairo_int_status_t status;

    status = _cairo_matrix_to_pixman_matrix_offset (&pattern->matrix,
						    pattern->filter,
						    extents->x + extents->width/2.,
						    extents->y + extents->height/2.,
						    &pixman_transform, ix, iy);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
    {
	/* If the transform is an identity, we don't need to set it
	 * and we can use any filtering, so choose the fastest one. */
	pixman_image_set_filter (pixman_image, PIXMAN_FILTER_NEAREST, NULL, 0);
    }
    else if (unlikely (status != CAIRO_INT_STATUS_SUCCESS ||
		       ! pixman_image_set_transform (pixman_image,
						     &pixman_transform)))
    {
	return FALSE;
    }
    else
    {
	pixman_filter_t pixman_filter;

	switch (pattern->filter) {
	case CAIRO_FILTER_FAST:
	    pixman_filter = PIXMAN_FILTER_FAST;
	    break;
	case CAIRO_FILTER_GOOD:
	    pixman_filter = PIXMAN_FILTER_GOOD;
	    break;
	case CAIRO_FILTER_BEST:
	    pixman_filter = PIXMAN_FILTER_BEST;
	    break;
	case CAIRO_FILTER_NEAREST:
	    pixman_filter = PIXMAN_FILTER_NEAREST;
	    break;
	case CAIRO_FILTER_BILINEAR:
	    pixman_filter = PIXMAN_FILTER_BILINEAR;
	    break;
	case CAIRO_FILTER_GAUSSIAN:
	    /* XXX: The GAUSSIAN value has no implementation in cairo
	     * whatsoever, so it was really a mistake to have it in the
	     * API. We could fix this by officially deprecating it, or
	     * else inventing semantics and providing an actual
	     * implementation for it. */
	default:
	    pixman_filter = PIXMAN_FILTER_BEST;
	}

	pixman_image_set_filter (pixman_image, pixman_filter, NULL, 0);
    }

    {
	pixman_repeat_t pixman_repeat;

	switch (pattern->extend) {
	default:
	case CAIRO_EXTEND_NONE:
	    pixman_repeat = PIXMAN_REPEAT_NONE;
	    break;
	case CAIRO_EXTEND_REPEAT:
	    pixman_repeat = PIXMAN_REPEAT_NORMAL;
	    break;
	case CAIRO_EXTEND_REFLECT:
	    pixman_repeat = PIXMAN_REPEAT_REFLECT;
	    break;
	case CAIRO_EXTEND_PAD:
	    pixman_repeat = PIXMAN_REPEAT_PAD;
	    break;
	}

	pixman_image_set_repeat (pixman_image, pixman_repeat);
    }

    if (pattern->has_component_alpha)
	pixman_image_set_component_alpha (pixman_image, TRUE);

    return TRUE;
}

static pixman_image_t *
_pixman_image_for_recording (cairo_image_surface_t *dst,
			     const cairo_surface_pattern_t *pattern,
			     cairo_bool_t is_mask,
			     const cairo_rectangle_int_t *extents,
			     const cairo_rectangle_int_t *sample,
			     int *ix, int *iy)
{
    cairo_surface_t *source, *clone;
    cairo_rectangle_int_t limit;
    pixman_image_t *pixman_image;
    cairo_status_t status;
    cairo_extend_t extend;
    cairo_matrix_t *m, matrix;
    int tx = 0, ty = 0;

    *ix = *iy = 0;

    source = pattern->surface;
    if (_cairo_surface_is_subsurface (source))
	source = _cairo_surface_subsurface_get_target_with_offset (source, &tx, &ty);
    if (_cairo_surface_is_snapshot (source))
	source = _cairo_surface_snapshot_get_target (source);
    if (_cairo_surface_is_observer (source))
	source = _cairo_surface_observer_get_target (source);
    if (_cairo_surface_is_paginated (source))
	source = _cairo_paginated_surface_get_target (source);

    extend = pattern->base.extend;
    if (_cairo_surface_get_extents (source, &limit)) {
	if (sample->x >= limit.x &&
	    sample->y >= limit.y &&
	    sample->x + sample->width  <= limit.x + limit.width &&
	    sample->y + sample->height <= limit.y + limit.height)
	{
	    extend = CAIRO_EXTEND_NONE;
	}
	else if (extend == CAIRO_EXTEND_NONE &&
		 (sample->x + sample->width <= limit.x ||
		  sample->x >= limit.x + limit.width ||
		  sample->y + sample->height <= limit.y ||
		  sample->y >= limit.y + limit.height))
	{
	    return _pixman_transparent_image ();
	}
    } else
	extend = CAIRO_EXTEND_NONE;

    if (extend == CAIRO_EXTEND_NONE) {
	if (! _cairo_rectangle_intersect (&limit, sample))
	    return _pixman_transparent_image ();

	if (! _cairo_matrix_is_identity (&pattern->base.matrix)) {
	    double x1, y1, x2, y2;

	    matrix = pattern->base.matrix;
	    status = cairo_matrix_invert (&matrix);
	    assert (status == CAIRO_STATUS_SUCCESS);

	    x1 = limit.x;
	    y1 = limit.y;
	    x2 = limit.x + limit.width;
	    y2 = limit.y + limit.height;

	    _cairo_matrix_transform_bounding_box (&matrix,
						  &x1, &y1, &x2, &y2, NULL);

	    limit.x = floor (x1);
	    limit.y = floor (y1);
	    limit.width  = ceil (x2) - limit.x;
	    limit.height = ceil (y2) - limit.y;
	}
    }

    if (dst->base.content == source->content)
	clone = cairo_image_surface_create (dst->format,
					    limit.width, limit.height);
    else
	clone = _cairo_image_surface_create_with_content (source->content,
							  limit.width,
							  limit.height);
    cairo_surface_set_device_offset (clone, -limit.x, -limit.y);

    m = NULL;
    if (extend == CAIRO_EXTEND_NONE) {
	m = &matrix;
	cairo_matrix_multiply (m,
			       &dst->base.device_transform,
			       &pattern->base.matrix);
	if (tx | ty)
	    cairo_matrix_translate (m, tx, ty);
    } else {
	/* XXX extract scale factor for repeating patterns */
    }

    status = _cairo_recording_surface_replay_with_clip (source, m, clone, NULL);
    if (unlikely (status)) {
	cairo_surface_destroy (clone);
	return NULL;
    }

    pixman_image = pixman_image_ref (((cairo_image_surface_t *)clone)->pixman_image);
    cairo_surface_destroy (clone);

    if (extend != CAIRO_EXTEND_NONE) {
	if (! _pixman_image_set_properties (pixman_image,
					    &pattern->base, extents,
					    ix, iy)) {
	    pixman_image_unref (pixman_image);
	    pixman_image= NULL;
	}
    } else {
	*ix = -limit.x;
	*iy = -limit.y;
    }

    return pixman_image;
}

static pixman_image_t *
_pixman_image_for_surface (cairo_image_surface_t *dst,
			   const cairo_surface_pattern_t *pattern,
			   cairo_bool_t is_mask,
			   const cairo_rectangle_int_t *extents,
			   const cairo_rectangle_int_t *sample,
			   int *ix, int *iy)
{
    cairo_extend_t extend = pattern->base.extend;
    pixman_image_t *pixman_image;

    *ix = *iy = 0;
    pixman_image = NULL;
    if (pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING)
	return _pixman_image_for_recording(dst, pattern,
					   is_mask, extents, sample,
					   ix, iy);

    if (pattern->surface->type == CAIRO_SURFACE_TYPE_IMAGE &&
	(! is_mask || ! pattern->base.has_component_alpha ||
	 (pattern->surface->content & CAIRO_CONTENT_COLOR) == 0))
    {
	cairo_image_surface_t *source = (cairo_image_surface_t *) pattern->surface;
	cairo_surface_type_t type;

	if (_cairo_surface_is_snapshot (&source->base))
	    source = (cairo_image_surface_t *) _cairo_surface_snapshot_get_target (&source->base);

	type = source->base.backend->type;
	if (type == CAIRO_SURFACE_TYPE_IMAGE) {
	    if (extend != CAIRO_EXTEND_NONE &&
		sample->x >= 0 &&
		sample->y >= 0 &&
		sample->x + sample->width  <= source->width &&
		sample->y + sample->height <= source->height)
	    {
		extend = CAIRO_EXTEND_NONE;
	    }

	    if (sample->width == 1 && sample->height == 1) {
		if (sample->x < 0 ||
		    sample->y < 0 ||
		    sample->x >= source->width ||
		    sample->y >= source->height)
		{
		    if (extend == CAIRO_EXTEND_NONE)
			return _pixman_transparent_image ();
		}
		else
		{
		    pixman_image = _pixel_to_solid (source,
						    sample->x, sample->y);
                    if (pixman_image)
                        return pixman_image;
		}
	    }

#if PIXMAN_HAS_ATOMIC_OPS
	    /* avoid allocating a 'pattern' image if we can reuse the original */
	    *ix = *iy = 0;
	    if (extend == CAIRO_EXTEND_NONE &&
		_cairo_matrix_is_pixman_translation (&pattern->base.matrix,
						     pattern->base.filter,
						     ix, iy))
	    {
		return pixman_image_ref (source->pixman_image);
	    }
#endif

	    pixman_image = pixman_image_create_bits (source->pixman_format,
						     source->width,
						     source->height,
						     (uint32_t *) source->data,
						     source->stride);
	    if (unlikely (pixman_image == NULL))
		return NULL;
	} else if (type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub;
	    cairo_bool_t is_contained = FALSE;

	    sub = (cairo_surface_subsurface_t *) source;
	    source = (cairo_image_surface_t *) sub->target;

	    if (sample->x >= 0 &&
		sample->y >= 0 &&
		sample->x + sample->width  <= sub->extents.width &&
		sample->y + sample->height <= sub->extents.height)
	    {
		is_contained = TRUE;
	    }

	    if (sample->width == 1 && sample->height == 1) {
		if (is_contained) {
		    pixman_image = _pixel_to_solid (source,
                                                    sub->extents.x + sample->x,
                                                    sub->extents.y + sample->y);
                    if (pixman_image)
                        return pixman_image;
		} else {
		    if (extend == CAIRO_EXTEND_NONE)
			return _pixman_transparent_image ();
		}
	    }

#if PIXMAN_HAS_ATOMIC_OPS
	    *ix = sub->extents.x;
	    *iy = sub->extents.y;
	    if (is_contained &&
		_cairo_matrix_is_pixman_translation (&pattern->base.matrix,
						     pattern->base.filter,
						     ix, iy))
	    {
		return pixman_image_ref (source->pixman_image);
	    }
#endif

	    /* Avoid sub-byte offsets, force a copy in that case. */
	    if (PIXMAN_FORMAT_BPP (source->pixman_format) >= 8) {
		void *data = source->data
		    + sub->extents.x * PIXMAN_FORMAT_BPP(source->pixman_format)/8
		    + sub->extents.y * source->stride;
		pixman_image = pixman_image_create_bits (source->pixman_format,
							 sub->extents.width,
							 sub->extents.height,
							 data,
							 source->stride);
		if (unlikely (pixman_image == NULL))
		    return NULL;
	    }
	}
    }

#if PIXMAN_HAS_ATOMIC_OPS
    *ix = *iy = 0;
#endif
    if (pixman_image == NULL) {
	struct acquire_source_cleanup *cleanup;
	cairo_image_surface_t *image;
	void *extra;
	cairo_status_t status;

	status = _cairo_surface_acquire_source_image (pattern->surface, &image, &extra);
	if (unlikely (status))
	    return NULL;

	if (sample->x >= 0 && sample->y >= 0 &&
	    sample->x + sample->width  <= image->width &&
	    sample->y + sample->height <= image->height)
	{
	    extend = CAIRO_EXTEND_NONE;
	}

	if (sample->width == 1 && sample->height == 1) {
	    if (sample->x < 0 ||
		sample->y < 0 ||
		sample->x >= image->width ||
		sample->y >= image->height)
	    {
		if (extend == CAIRO_EXTEND_NONE) {
		    pixman_image = _pixman_transparent_image ();
		    _cairo_surface_release_source_image (pattern->surface, image, extra);
		    return pixman_image;
		}
	    }
	    else
	    {
		pixman_image = _pixel_to_solid (image, sample->x, sample->y);
                if (pixman_image) {
                    _cairo_surface_release_source_image (pattern->surface, image, extra);
                    return pixman_image;
                }
	    }
	}

	pixman_image = pixman_image_create_bits (image->pixman_format,
						 image->width,
						 image->height,
						 (uint32_t *) image->data,
						 image->stride);
	if (unlikely (pixman_image == NULL)) {
	    _cairo_surface_release_source_image (pattern->surface, image, extra);
	    return NULL;
	}

	cleanup = malloc (sizeof (*cleanup));
	if (unlikely (cleanup == NULL)) {
	    _cairo_surface_release_source_image (pattern->surface, image, extra);
	    pixman_image_unref (pixman_image);
	    return NULL;
	}

	cleanup->surface = pattern->surface;
	cleanup->image = image;
	cleanup->image_extra = extra;
	pixman_image_set_destroy_function (pixman_image,
					   _acquire_source_cleanup, cleanup);
    }

    if (! _pixman_image_set_properties (pixman_image,
					&pattern->base, extents,
					ix, iy)) {
	pixman_image_unref (pixman_image);
	pixman_image= NULL;
    }

    return pixman_image;
}

pixman_image_t *
_pixman_image_for_pattern (cairo_image_surface_t *dst,
			   const cairo_pattern_t *pattern,
			   cairo_bool_t is_mask,
			   const cairo_rectangle_int_t *extents,
			   const cairo_rectangle_int_t *sample,
			   int *tx, int *ty)
{
    *tx = *ty = 0;

    if (pattern == NULL)
	return _pixman_white_image ();

    switch (pattern->type) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_PATTERN_TYPE_SOLID:
	return _pixman_image_for_color (&((const cairo_solid_pattern_t *) pattern)->color);

    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_LINEAR:
	return _pixman_image_for_gradient ((const cairo_gradient_pattern_t *) pattern,
					   extents, tx, ty);

    case CAIRO_PATTERN_TYPE_MESH:
	return _pixman_image_for_mesh ((const cairo_mesh_pattern_t *) pattern,
					   extents, tx, ty);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _pixman_image_for_surface (dst,
					  (const cairo_surface_pattern_t *) pattern,
					  is_mask, extents, sample,
					  tx, ty);

    }
}

static cairo_status_t
_cairo_image_source_finish (void *abstract_surface)
{
    cairo_image_source_t *source = abstract_surface;

    pixman_image_unref (source->pixman_image);
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_image_source_backend = {
    CAIRO_SURFACE_TYPE_IMAGE,
    _cairo_image_source_finish,
    NULL, /* read-only wrapper */
};

cairo_surface_t *
_cairo_image_source_create_for_pattern (cairo_surface_t *dst,
					 const cairo_pattern_t *pattern,
					 cairo_bool_t is_mask,
					 const cairo_rectangle_int_t *extents,
					 const cairo_rectangle_int_t *sample,
					 int *src_x, int *src_y)
{
    cairo_image_source_t *source;

    source = malloc (sizeof (cairo_image_source_t));
    if (unlikely (source == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    source->pixman_image =
	_pixman_image_for_pattern ((cairo_image_surface_t *)dst,
				   pattern, is_mask,
				   extents, sample,
				   src_x, src_y);
    if (unlikely (source->pixman_image == NULL)) {
	free (source);
	return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_surface_init (&source->base,
			 &cairo_image_source_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA);

    source->is_opaque_solid =
	pattern == NULL || _cairo_pattern_is_opaque_solid (pattern);

    return &source->base;
}
