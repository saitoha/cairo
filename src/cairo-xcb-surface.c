/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2009 Intel Corporation
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
 *	Behdad Esfahbod <behdad@behdad.org>
 *	Carl D. Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 */

#include "cairoint.h"

#include "cairo-xcb.h"
#include "cairo-xcb-private.h"

#include "cairo-default-context-private.h"

#define XLIB_COORD_MAX 32767

#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_proto (cairo_xcb_surface_create);
slim_hidden_proto (cairo_xcb_surface_create_for_bitmap);
slim_hidden_proto (cairo_xcb_surface_create_with_xrender_format);
#endif

/**
 * SECTION:cairo-xcb
 * @Title: XCB Surfaces
 * @Short_Description: X Window System rendering using the XCB library
 * @See_Also: #cairo_surface_t
 *
 * The XCB surface is used to render cairo graphics to X Window System
 * windows and pixmaps using the XCB library.
 *
 * Note that the XCB surface automatically takes advantage of the X render
 * extension if it is available.
 */

/**
 * CAIRO_HAS_XCB_SURFACE:
 *
 * Defined if the xcb surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

cairo_surface_t *
_cairo_xcb_surface_create_similar (void			*abstract_other,
				   cairo_content_t	 content,
				   int			 width,
				   int			 height)
{
    cairo_xcb_surface_t *other = abstract_other;
    cairo_xcb_surface_t *surface;
    cairo_xcb_connection_t *connection;
    xcb_pixmap_t pixmap;
    cairo_status_t status;

    if (unlikely(width  > XLIB_COORD_MAX ||
		 height > XLIB_COORD_MAX ||
		 width  <= 0 ||
		 height <= 0))
	return cairo_image_surface_create (_cairo_format_from_content (content),
					   width, height);

    if ((other->connection->flags & CAIRO_XCB_HAS_RENDER) == 0)
	return _cairo_xcb_surface_create_similar_image (other,
							_cairo_format_from_content (content),
							width, height);

    connection = other->connection;
    status = _cairo_xcb_connection_acquire (connection);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    if (content == other->base.content) {
	pixmap = _cairo_xcb_connection_create_pixmap (connection,
						      other->depth,
						      other->drawable,
						      width, height);

	surface = (cairo_xcb_surface_t *)
	    _cairo_xcb_surface_create_internal (other->screen,
						pixmap, TRUE,
						other->pixman_format,
						other->xrender_format,
						width, height);
    } else {
	cairo_format_t format;
	pixman_format_code_t pixman_format;

	/* XXX find a compatible xrender format */
	switch (content) {
	case CAIRO_CONTENT_ALPHA:
	    pixman_format = PIXMAN_a8;
	    format = CAIRO_FORMAT_A8;
	    break;
	case CAIRO_CONTENT_COLOR:
	    pixman_format = PIXMAN_x8r8g8b8;
	    format = CAIRO_FORMAT_RGB24;
	    break;
	default:
	    ASSERT_NOT_REACHED;
	case CAIRO_CONTENT_COLOR_ALPHA:
	    pixman_format = PIXMAN_a8r8g8b8;
	    format = CAIRO_FORMAT_ARGB32;
	    break;
	}

	pixmap = _cairo_xcb_connection_create_pixmap (connection,
						      PIXMAN_FORMAT_DEPTH (pixman_format),
						      other->drawable,
						      width, height);

	surface = (cairo_xcb_surface_t *)
	    _cairo_xcb_surface_create_internal (other->screen,
						pixmap, TRUE,
						pixman_format,
						connection->standard_formats[format],
						width, height);
    }

    if (unlikely (surface->base.status))
	_cairo_xcb_connection_free_pixmap (connection, pixmap);

    _cairo_xcb_connection_release (connection);

    return &surface->base;
}

cairo_surface_t *
_cairo_xcb_surface_create_similar_image (void			*abstract_other,
					 cairo_format_t		 format,
					 int			 width,
					 int			 height)
{
#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    cairo_xcb_surface_t *other = abstract_other;
    cairo_xcb_connection_t *connection = other->connection;

    cairo_surface_t *image;
    cairo_xcb_shm_info_t *shm_info;
    cairo_status_t status;
    size_t stride;
    pixman_format_code_t pixman_format;

    if (unlikely(width > XLIB_COORD_MAX || height > XLIB_COORD_MAX))
	return cairo_image_surface_create (format, width, height);

    if ((connection->flags & CAIRO_XCB_HAS_SHM) == 0)
	return cairo_image_surface_create (format, width, height);

    pixman_format = _cairo_format_to_pixman_format_code (format);

    stride = CAIRO_STRIDE_FOR_WIDTH_BPP (width,
					 PIXMAN_FORMAT_BPP (pixman_format));
    status = _cairo_xcb_connection_allocate_shm_info (connection,
						      stride * height,
						      &shm_info);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    image = _cairo_image_surface_create_with_pixman_format (shm_info->mem,
							    pixman_format,
							    width,
							    height,
							    stride);
    if (unlikely (image->status)) {
	_cairo_xcb_shm_info_destroy (shm_info);
	return image;
    }

    status = _cairo_user_data_array_set_data (&image->user_data,
					      (const cairo_user_data_key_t *) connection,
					      shm_info,
					      (cairo_destroy_func_t) _cairo_xcb_shm_info_destroy);
    if (unlikely (status)) {
	cairo_surface_destroy (image);
	_cairo_xcb_shm_info_destroy (shm_info);
	return _cairo_surface_create_in_error (status);
    }

    return image;
#else
    return cairo_image_surface_create (format, width, height);
#endif
}

static cairo_status_t
_cairo_xcb_surface_finish (void *abstract_surface)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->fallback != NULL) {
	cairo_surface_finish (&surface->fallback->base);
	cairo_surface_destroy (&surface->fallback->base);
    }

    cairo_list_del (&surface->link);

    status = _cairo_xcb_connection_acquire (surface->connection);
    if (status == CAIRO_STATUS_SUCCESS) {
	if (surface->picture != XCB_NONE) {
	    _cairo_xcb_connection_render_free_picture (surface->connection,
						       surface->picture);
	}

	if (surface->owns_pixmap)
	    _cairo_xcb_connection_free_pixmap (surface->connection, surface->drawable);
	_cairo_xcb_connection_release (surface->connection);
    }

    _cairo_xcb_connection_destroy (surface->connection);

    return status;
}

static void
_destroy_image (pixman_image_t *image, void *data)
{
    free (data);
}

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
static cairo_surface_t *
_cairo_xcb_surface_create_shm_image (cairo_xcb_connection_t *connection,
				     pixman_format_code_t pixman_format,
				     int width, int height,
				     cairo_xcb_shm_info_t **shm_info_out)
{
    cairo_surface_t *image;
    cairo_xcb_shm_info_t *shm_info;
    cairo_status_t status;
    size_t stride;

    *shm_info_out = NULL;

    stride = CAIRO_STRIDE_FOR_WIDTH_BPP (width,
					 PIXMAN_FORMAT_BPP (pixman_format));
    status = _cairo_xcb_connection_allocate_shm_info (connection,
						      stride * height,
						      &shm_info);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    image = _cairo_image_surface_create_with_pixman_format (shm_info->mem,
							    pixman_format,
							    width, height,
							    stride);
    if (unlikely (image->status)) {
	_cairo_xcb_shm_info_destroy (shm_info);
	return image;
    }

    status = _cairo_user_data_array_set_data (&image->user_data,
					      (const cairo_user_data_key_t *) connection,
					      shm_info,
					      (cairo_destroy_func_t) _cairo_xcb_shm_info_destroy);
    if (unlikely (status)) {
	cairo_surface_destroy (image);
	_cairo_xcb_shm_info_destroy (shm_info);
	return _cairo_surface_create_in_error (status);
    }

    *shm_info_out = shm_info;
    return image;
}
#endif

static cairo_image_surface_t *
_get_shm_image (cairo_xcb_surface_t *surface,
		int x, int y,
		int width, int height)
{
#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    cairo_xcb_shm_info_t *shm_info;
    cairo_surface_t *image;
    cairo_status_t status;

    if ((surface->connection->flags & CAIRO_XCB_HAS_SHM) == 0)
	return NULL;

    image = _cairo_xcb_surface_create_shm_image (surface->connection,
						 surface->pixman_format,
						 width, height,
						 &shm_info);
    if (unlikely (image == NULL || image->status))
	goto done;

    status = _cairo_xcb_connection_shm_get_image (surface->connection,
						  surface->drawable,
						  x, y,
						  width, height,
						  shm_info->shm,
						  shm_info->offset);
    if (unlikely (status)) {
	cairo_surface_destroy (image);
	image = _cairo_surface_create_in_error (status);
    }

done:
    return (cairo_image_surface_t *) image;
#else
    return NULL;;
#endif
}

static cairo_image_surface_t *
_get_image (cairo_xcb_surface_t		 *surface,
	    cairo_bool_t		  use_shm,
	    int x, int y,
	    int width, int height)
{
    cairo_image_surface_t *image;
    cairo_xcb_connection_t *connection;
    xcb_get_image_reply_t *reply;
    cairo_int_status_t status;

    assert (surface->fallback == NULL);
    assert (x >= 0);
    assert (y >= 0);
    assert (x + width <= surface->width);
    assert (y + height <= surface->height);

    if (surface->deferred_clear) {
	image = (cairo_image_surface_t *)
	    _cairo_image_surface_create_with_pixman_format (NULL,
							    surface->pixman_format,
							    width, height,
							    0);
	if (surface->deferred_clear_color.alpha_short > 0x00ff) {
	    cairo_solid_pattern_t solid;

	    _cairo_pattern_init_solid (&solid, &surface->deferred_clear_color);
	    status = _cairo_surface_paint (&image->base,
					   CAIRO_OPERATOR_SOURCE,
					   &solid.base,
					   NULL);
	    if (unlikely (status)) {
		cairo_surface_destroy (&image->base);
		image = (cairo_image_surface_t *)
		    _cairo_surface_create_in_error (status);
	    }
	}
	return image;
    }

    connection = surface->connection;

    status = _cairo_xcb_connection_acquire (connection);
    if (unlikely (status))
	return (cairo_image_surface_t *) _cairo_surface_create_in_error (status);

    if (use_shm) {
	image = _get_shm_image (surface, x, y, width, height);
	if (image) {
	    _cairo_xcb_connection_release (connection);
	    return image;
	}
    }

    if (surface->use_pixmap == 0) {
	status = _cairo_xcb_connection_get_image (connection,
						  surface->drawable,
						  x, y,
						  width, height,
						  &reply);
	if (unlikely (status))
	    goto FAIL;
    } else {
	surface->use_pixmap--;
	reply = NULL;
    }

    if (reply == NULL && ! surface->owns_pixmap) {
	/* xcb_get_image_t from a window is dangerous because it can
	 * produce errors if the window is unmapped or partially
	 * outside the screen. We could check for errors and
	 * retry, but to keep things simple, we just create a
	 * temporary pixmap
	 */
	xcb_pixmap_t pixmap;
	xcb_gcontext_t gc;

	gc = _cairo_xcb_screen_get_gc (surface->screen,
				       surface->drawable,
				       surface->depth);
	pixmap = _cairo_xcb_connection_create_pixmap (connection,
						      surface->depth,
						      surface->drawable,
						      width, height);

	/* XXX IncludeInferiors? */
	_cairo_xcb_connection_copy_area (connection,
					 surface->drawable,
					 pixmap, gc,
					 x, y,
					 0, 0,
					 width, height);

	_cairo_xcb_screen_put_gc (surface->screen, surface->depth, gc);

	status = _cairo_xcb_connection_get_image (connection,
						  pixmap,
						  0, 0,
						  width, height,
						  &reply);
	_cairo_xcb_connection_free_pixmap (connection, pixmap);

	if (unlikely (status))
	    goto FAIL;
    }

    if (unlikely (reply == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto FAIL;
    }

    /* XXX byte swap */
    /* XXX format conversion */
    assert (reply->depth == surface->depth);

    image = (cairo_image_surface_t *)
	_cairo_image_surface_create_with_pixman_format
	(xcb_get_image_data (reply),
	 surface->pixman_format,
	 width, height,
	 CAIRO_STRIDE_FOR_WIDTH_BPP (width,
				     PIXMAN_FORMAT_BPP (surface->pixman_format)));
    status = image->base.status;
    if (unlikely (status)) {
	free (reply);
	goto FAIL;
    }

    assert (xcb_get_image_data_length (reply) == image->height * image->stride);

    pixman_image_set_destroy_function (image->pixman_image, _destroy_image, reply);

    _cairo_xcb_connection_release (connection);

    return image;

FAIL:
    _cairo_xcb_connection_release (connection);
    return (cairo_image_surface_t *) _cairo_surface_create_in_error (status);
}

static cairo_status_t
_cairo_xcb_surface_acquire_source_image (void *abstract_surface,
					 cairo_image_surface_t **image_out,
					 void **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;

    if (surface->fallback != NULL) {
	image = (cairo_image_surface_t *) cairo_surface_reference (&surface->fallback->base);
	goto DONE;
    }

    image = (cairo_image_surface_t *)
	_cairo_surface_has_snapshot (&surface->base,
				     &_cairo_image_surface_backend);
    if (image != NULL) {
	image = (cairo_image_surface_t *) cairo_surface_reference (&image->base);
	goto DONE;
    }

    image = _get_image (surface, FALSE, 0, 0, surface->width, surface->height);
    if (unlikely (image->base.status))
	return image->base.status;

    _cairo_surface_attach_snapshot (&surface->base, &image->base, NULL);

DONE:
    *image_out = image;
    *image_extra = NULL;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_release_source_image (void *abstract_surface,
					 cairo_image_surface_t *image,
					 void *image_extra)
{
    cairo_surface_destroy (&image->base);
}

cairo_bool_t
_cairo_xcb_surface_get_extents (void *abstract_surface,
				cairo_rectangle_int_t *extents)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    extents->x = extents->y = 0;
    extents->width  = surface->width;
    extents->height = surface->height;
    return TRUE;
}

static void
_cairo_xcb_surface_get_font_options (void *abstract_surface,
				     cairo_font_options_t *options)
{
    /* XXX  copy from xlib */
    _cairo_font_options_init_default (options);
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_ON);
}

static cairo_status_t
_put_shm_image (cairo_xcb_surface_t    *surface,
		xcb_gcontext_t		gc,
		cairo_image_surface_t  *image)
{
#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    cairo_xcb_shm_info_t *shm_info;

    shm_info = _cairo_user_data_array_get_data (&image->base.user_data,
						(const cairo_user_data_key_t *) surface->connection);
    if (shm_info == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_xcb_connection_shm_put_image (surface->connection,
					 surface->drawable,
					 gc,
					 surface->width, surface->height,
					 image->base.device_transform_inverse.x0,
					 image->base.device_transform_inverse.y0,
					 image->width, image->height,
					 0, 0,
					 image->depth,
					 shm_info->shm,
					 shm_info->offset);

    return CAIRO_STATUS_SUCCESS;
#else
    return CAIRO_INT_STATUS_UNSUPPORTED;
#endif
}

static cairo_status_t
_put_image (cairo_xcb_surface_t    *surface,
	    cairo_image_surface_t  *image)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;

    /* XXX track damaged region? */

    status = _cairo_xcb_connection_acquire (surface->connection);
    if (unlikely (status))
	return status;

    if (image->pixman_format == surface->pixman_format) {
	xcb_gcontext_t gc;

	assert (image->depth == surface->depth);
	assert (image->stride == (int) CAIRO_STRIDE_FOR_WIDTH_BPP (image->width, PIXMAN_FORMAT_BPP (image->pixman_format)));

	gc = _cairo_xcb_screen_get_gc (surface->screen,
				       surface->drawable,
				       surface->depth);

	status = _put_shm_image (surface, gc, image);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_xcb_connection_put_image (surface->connection,
					     surface->drawable, gc,
					     image->width, image->height,
					     image->base.device_transform_inverse.x0,
					     image->base.device_transform_inverse.y0,
					     image->depth,
					     image->stride,
					     image->data);
	    status = CAIRO_STATUS_SUCCESS;
	}

	_cairo_xcb_screen_put_gc (surface->screen, surface->depth, gc);
    } else {
	ASSERT_NOT_REACHED;
    }

    _cairo_xcb_connection_release (surface->connection);
    return status;
}

static cairo_status_t
_cairo_xcb_surface_flush (void *abstract_surface)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (likely (surface->fallback == NULL)) {
	status = CAIRO_STATUS_SUCCESS;
	if (! surface->base.finished && surface->deferred_clear)
	    status = _cairo_xcb_surface_clear (surface);

	return status;
    }

    status = surface->base.status;
    if (status == CAIRO_STATUS_SUCCESS && ! surface->base.finished) {
	status = cairo_surface_status (&surface->fallback->base);

	if (status == CAIRO_STATUS_SUCCESS) {
	    status = _put_image (surface, surface->fallback);
	}

	if (status == CAIRO_STATUS_SUCCESS) {
	    _cairo_surface_attach_snapshot (&surface->base,
					    &surface->fallback->base,
					    cairo_surface_finish);
	}
    }

    cairo_surface_destroy (&surface->fallback->base);
    surface->fallback = NULL;

    return status;
}

static cairo_surface_t *
_cairo_xcb_surface_map_to_image (void *abstract_surface,
				 const cairo_rectangle_int_t *extents)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;

    if (surface->fallback)
	return surface->fallback->base.backend->map_to_image (surface->fallback, extents);

    image = _get_image (surface, TRUE,
			extents->x, extents->y,
			extents->width, extents->height);
    if (unlikely (image->base.status))
	return &image->base;

    /* Do we have a deferred clear and this image surface does NOT cover the
     * whole xcb surface? Have to apply the clear in that case, else
     * uploading the image will handle the problem for us.
     */
    if (surface->deferred_clear &&
	    ! (extents->x == 0 &&
	       extents->y == 0 &&
	       extents->width == surface->width &&
	       extents->height == surface->height)) {
	cairo_status_t status = _cairo_xcb_surface_clear (surface);
	if (unlikely (status)) {
	    cairo_surface_destroy(&image->base);
	    return _cairo_surface_create_in_error (status);
	}
    }
    surface->deferred_clear = FALSE;

    cairo_surface_set_device_offset (&image->base, -extents->x, -extents->y);
    return &image->base;
}

static cairo_int_status_t
_cairo_xcb_surface_unmap (void *abstract_surface,
			  cairo_image_surface_t *image)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    if (surface->fallback)
	return surface->fallback->base.backend->unmap_image (surface->fallback, image);
    return _put_image (abstract_surface, image);
}

static cairo_image_surface_t *
_cairo_xcb_surface_fallback (cairo_xcb_surface_t *surface)
{
    cairo_image_surface_t *image;
    image = _get_image (surface, TRUE, 0, 0, surface->width, surface->height);

    /* If there was a deferred clear, _get_image applied it */
    if (image->base.status == CAIRO_STATUS_SUCCESS)
	surface->deferred_clear = FALSE;

    return image;
}

static cairo_int_status_t
_cairo_xcb_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  const cairo_clip_t	*clip)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    if (surface->fallback == NULL) {
	status = _cairo_xcb_surface_cairo_paint (surface, op, source, clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	status = _cairo_xcb_surface_render_paint (surface, op, source, clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	surface->fallback = _cairo_xcb_surface_fallback (surface);
    }

    return _cairo_surface_paint (&surface->fallback->base, op, source, clip);
}

static cairo_int_status_t
_cairo_xcb_surface_mask (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_pattern_t	*mask,
			 const cairo_clip_t	*clip)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    if (surface->fallback == NULL) {
	status =  _cairo_xcb_surface_cairo_mask (surface,
						 op, source, mask, clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	status =  _cairo_xcb_surface_render_mask (surface,
						  op, source, mask, clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	surface->fallback = _cairo_xcb_surface_fallback (surface);
    }

    return _cairo_surface_mask (&surface->fallback->base,
				op, source, mask,
				clip);
}

static cairo_int_status_t
_cairo_xcb_surface_stroke (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   const cairo_path_fixed_t	*path,
			   const cairo_stroke_style_t	*style,
			   const cairo_matrix_t		*ctm,
			   const cairo_matrix_t		*ctm_inverse,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   const cairo_clip_t		*clip)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    if (surface->fallback == NULL) {
	status = _cairo_xcb_surface_cairo_stroke (surface, op, source,
						  path, style,
						  ctm, ctm_inverse,
						  tolerance, antialias,
						  clip);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	status = _cairo_xcb_surface_render_stroke (surface, op, source,
						   path, style,
						   ctm, ctm_inverse,
						   tolerance, antialias,
						   clip);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	surface->fallback = _cairo_xcb_surface_fallback (surface);
    }

    return _cairo_surface_stroke (&surface->fallback->base,
				  op, source,
				  path, style,
				  ctm, ctm_inverse,
				  tolerance, antialias,
				  clip);
}

static cairo_int_status_t
_cairo_xcb_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_path_fixed_t*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias,
			 const cairo_clip_t	*clip)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    if (surface->fallback == NULL) {
	status = _cairo_xcb_surface_cairo_fill (surface, op, source,
						path, fill_rule,
						tolerance, antialias,
						clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	status = _cairo_xcb_surface_render_fill (surface, op, source,
						 path, fill_rule,
						 tolerance, antialias,
						 clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	surface->fallback = _cairo_xcb_surface_fallback (surface);
    }

    return _cairo_surface_fill (&surface->fallback->base,
				op, source,
				path, fill_rule,
				tolerance, antialias,
				clip);
}

static cairo_int_status_t
_cairo_xcb_surface_glyphs (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_glyph_t		*glyphs,
			   int				 num_glyphs,
			   cairo_scaled_font_t		*scaled_font,
			   const cairo_clip_t		*clip,
			   int *num_remaining)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    *num_remaining = 0;

    if (surface->fallback == NULL) {
	status = _cairo_xcb_surface_cairo_glyphs (surface,
						  op, source,
						  scaled_font, glyphs, num_glyphs,
						  clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	status = _cairo_xcb_surface_render_glyphs (surface,
						   op, source,
						   scaled_font, glyphs, num_glyphs,
						   clip);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	surface->fallback = _cairo_xcb_surface_fallback (surface);
    }

    return _cairo_surface_show_text_glyphs (&surface->fallback->base,
					    op, source,
					    NULL, 0,
					    glyphs, num_glyphs,
					    NULL, 0, 0,
					    scaled_font,
					    clip);
}

const cairo_surface_backend_t _cairo_xcb_surface_backend = {
    CAIRO_SURFACE_TYPE_XCB,
    _cairo_xcb_surface_finish,
    _cairo_default_context_create,

    _cairo_xcb_surface_create_similar,
    _cairo_xcb_surface_create_similar_image,

    _cairo_xcb_surface_map_to_image,
    _cairo_xcb_surface_unmap,

    _cairo_xcb_surface_acquire_source_image,
    _cairo_xcb_surface_release_source_image,

    NULL, NULL, NULL, /* dest acquire/release/clone */

    NULL, /* composite */
    NULL, /* fill */
    NULL, /* trapezoids */
    NULL, /* span */
    NULL, /* check-span */

    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_xcb_surface_get_extents,
    NULL, /* old-glyphs */
    _cairo_xcb_surface_get_font_options,

    _cairo_xcb_surface_flush,
    NULL,
    _cairo_xcb_surface_scaled_font_fini,
    _cairo_xcb_surface_scaled_glyph_fini,

    _cairo_xcb_surface_paint,
    _cairo_xcb_surface_mask,
    _cairo_xcb_surface_stroke,
    _cairo_xcb_surface_fill,
    _cairo_xcb_surface_glyphs,
};

cairo_surface_t *
_cairo_xcb_surface_create_internal (cairo_xcb_screen_t		*screen,
				    xcb_drawable_t		 drawable,
				    cairo_bool_t		 owns_pixmap,
				    pixman_format_code_t	 pixman_format,
				    xcb_render_pictformat_t	 xrender_format,
				    int				 width,
				    int				 height)
{
    cairo_xcb_surface_t *surface;

    surface = malloc (sizeof (cairo_xcb_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_xcb_surface_backend,
			 &screen->connection->device,
			 _cairo_content_from_pixman_format (pixman_format));

    surface->connection = _cairo_xcb_connection_reference (screen->connection);
    surface->screen = screen;
    cairo_list_add (&surface->link, &screen->surfaces);

    surface->fallback = NULL;

    surface->drawable = drawable;
    surface->owns_pixmap = owns_pixmap;
    surface->use_pixmap = 0;

    surface->deferred_clear = FALSE;
    surface->deferred_clear_color = *CAIRO_COLOR_TRANSPARENT;

    surface->width  = width;
    surface->height = height;
    surface->depth  = PIXMAN_FORMAT_DEPTH (pixman_format);

    surface->picture = XCB_NONE;
    if (screen->connection->force_precision != -1)
	surface->precision = screen->connection->force_precision;
    else
	surface->precision = XCB_RENDER_POLY_MODE_IMPRECISE;

    surface->pixman_format = pixman_format;
    surface->xrender_format = xrender_format;

    return &surface->base;
}

static xcb_screen_t *
_cairo_xcb_screen_from_visual (xcb_connection_t *connection,
			       xcb_visualtype_t *visual,
			       int *depth)
{
    xcb_depth_iterator_t d;
    xcb_screen_iterator_t s;

    s = xcb_setup_roots_iterator (xcb_get_setup (connection));
    for (; s.rem; xcb_screen_next (&s)) {
	if (s.data->root_visual == visual->visual_id) {
	    *depth = s.data->root_depth;
	    return s.data;
	}

	d = xcb_screen_allowed_depths_iterator(s.data);
	for (; d.rem; xcb_depth_next (&d)) {
	    xcb_visualtype_iterator_t v = xcb_depth_visuals_iterator (d.data);

	    for (; v.rem; xcb_visualtype_next (&v)) {
		if (v.data->visual_id == visual->visual_id) {
		    *depth = d.data->depth;
		    return s.data;
		}
	    }
	}
    }

    return NULL;
}

/**
 * cairo_xcb_surface_create:
 * @xcb_connection: an XCB connection
 * @drawable: an XCB drawable
 * @visual: the visual to use for drawing to @drawable. The depth
 *          of the visual must match the depth of the drawable.
 *          Currently, only TrueColor visuals are fully supported.
 * @width: the current width of @drawable
 * @height: the current height of @drawable
 *
 * Creates an XCB surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided visual.
 *
 * Note: If @drawable is a Window, then the function
 * cairo_xcb_surface_set_size() must be called whenever the size of the
 * window changes.
 *
 * When @drawable is a Window containing child windows then drawing to
 * the created surface will be clipped by those child windows.  When
 * the created surface is used as a source, the contents of the
 * children will be included.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_xcb_surface_create (xcb_connection_t  *xcb_connection,
			  xcb_drawable_t     drawable,
			  xcb_visualtype_t  *visual,
			  int		     width,
			  int		     height)
{
    cairo_xcb_screen_t *screen;
    xcb_screen_t *xcb_screen;
    cairo_format_masks_t image_masks;
    pixman_format_code_t pixman_format;
    xcb_render_pictformat_t xrender_format;
    int depth;

    if (xcb_connection_has_error (xcb_connection))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_WRITE_ERROR));

    if (unlikely (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    if (unlikely (width <= 0 || height <= 0))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    xcb_screen = _cairo_xcb_screen_from_visual (xcb_connection, visual, &depth);
    if (unlikely (xcb_screen == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_VISUAL));

    image_masks.alpha_mask = 0;
    image_masks.red_mask   = visual->red_mask;
    image_masks.green_mask = visual->green_mask;
    image_masks.blue_mask  = visual->blue_mask;
    if (depth == 32) /* XXX visuals have no alpha! */
	image_masks.alpha_mask =
	    0xffffffff & ~(visual->red_mask | visual->green_mask | visual->blue_mask);
    if (depth > 16)
	image_masks.bpp = 32;
    else if (depth > 8)
	image_masks.bpp = 16;
    else if (depth > 1)
	image_masks.bpp = 8;
    else
	image_masks.bpp = 1;

    if (! _pixman_format_from_masks (&image_masks, &pixman_format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    screen = _cairo_xcb_screen_get (xcb_connection, xcb_screen);
    if (unlikely (screen == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    xrender_format =
	_cairo_xcb_connection_get_xrender_format_for_visual (screen->connection,
							     visual->visual_id);

    return _cairo_xcb_surface_create_internal (screen, drawable, FALSE,
					       pixman_format,
					       xrender_format,
					       width, height);
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_surface_create);
#endif

/**
 * cairo_xcb_surface_create_for_bitmap:
 * @xcb_connection: an XCB connection
 * @xcb_screen: the XCB screen associated with @bitmap
 * @bitmap: an XCB drawable (a Pixmap with depth 1)
 * @width: the current width of @drawable
 * @height: the current height of @drawable
 *
 * Creates an XCB surface that draws to the given bitmap.
 * This will be drawn to as a %CAIRO_FORMAT_A1 object.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_xcb_surface_create_for_bitmap (xcb_connection_t	*xcb_connection,
				     xcb_screen_t	*xcb_screen,
				     xcb_pixmap_t	 bitmap,
				     int		 width,
				     int		 height)
{
    cairo_xcb_screen_t *screen;

    if (xcb_connection_has_error (xcb_connection))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_WRITE_ERROR));

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    if (unlikely (width <= 0 || height <= 0))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    screen = _cairo_xcb_screen_get (xcb_connection, xcb_screen);
    if (unlikely (screen == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    return _cairo_xcb_surface_create_internal (screen, bitmap, FALSE,
					       PIXMAN_a1,
					       screen->connection->standard_formats[CAIRO_FORMAT_A1],
					       width, height);
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_surface_create_for_bitmap);
#endif

/**
 * cairo_xcb_surface_create_with_xrender_format:
 * @connection: an XCB connection
 * @drawable: an XCB drawable
 * @screen: the XCB screen associated with @drawable
 * @format: the picture format to use for drawing to @drawable. The
 *          depth of @format mush match the depth of the drawable.
 * @width: the current width of @drawable
 * @height: the current height of @drawable
 *
 * Creates an XCB surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided picture format.
 *
 * Note: If @drawable is a Window, then the function
 * cairo_xcb_surface_set_size() must be called whenever the size of the
 * window changes.
 *
 * When @drawable is a Window containing child windows then drawing to
 * the created surface will be clipped by those child windows.  When
 * the created surface is used as a source, the contents of the
 * children will be included.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_xcb_surface_create_with_xrender_format (xcb_connection_t	    *xcb_connection,
					      xcb_screen_t	    *xcb_screen,
					      xcb_drawable_t	     drawable,
					      xcb_render_pictforminfo_t *format,
					      int		     width,
					      int		     height)
{
    cairo_xcb_screen_t *screen;
    cairo_format_masks_t image_masks;
    pixman_format_code_t pixman_format;

    if (xcb_connection_has_error (xcb_connection))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_WRITE_ERROR));

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    if (unlikely (width <= 0 || height <= 0))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    image_masks.alpha_mask =
	(unsigned long) format->direct.alpha_mask << format->direct.alpha_shift;
    image_masks.red_mask =
	(unsigned long) format->direct.red_mask << format->direct.red_shift;
    image_masks.green_mask =
	(unsigned long) format->direct.green_mask << format->direct.green_shift;
    image_masks.blue_mask =
	(unsigned long) format->direct.blue_mask << format->direct.blue_shift;
#if 0
    image_masks.bpp = format->depth;
#else
    if (format->depth > 16)
	image_masks.bpp = 32;
    else if (format->depth > 8)
	image_masks.bpp = 16;
    else if (format->depth > 1)
	image_masks.bpp = 8;
    else
	image_masks.bpp = 1;
#endif

    if (! _pixman_format_from_masks (&image_masks, &pixman_format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    screen = _cairo_xcb_screen_get (xcb_connection, xcb_screen);
    if (unlikely (screen == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    return _cairo_xcb_surface_create_internal (screen,
					       drawable,
					       FALSE,
					       pixman_format,
					       format->id,
					       width, height);
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_surface_create_with_xrender_format);
#endif

/**
 * cairo_xcb_surface_set_size:
 * @surface: a #cairo_surface_t for the XCB backend
 * @width: the new width of the surface
 * @height: the new height of the surface
 *
 * Informs cairo of the new size of the XCB drawable underlying the
 * surface. For a surface created for a window (rather than a pixmap),
 * this function must be called each time the size of the window
 * changes. (For a subwindow, you are normally resizing the window
 * yourself, but for a toplevel window, it is necessary to listen for
 * ConfigureNotify events.)
 *
 * A pixmap can never change size, so it is never necessary to call
 * this function on a surface created for a pixmap.
 **/
void
cairo_xcb_surface_set_size (cairo_surface_t *abstract_surface,
			    int              width,
			    int              height)
{
    cairo_xcb_surface_t *surface;
    cairo_status_t status_ignored;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status_ignored = _cairo_surface_set_error (abstract_surface,
						   _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return;
    }


    if (abstract_surface->type != CAIRO_SURFACE_TYPE_XCB) {
	status_ignored = _cairo_surface_set_error (abstract_surface,
						   _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return;
    }

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX || width <= 0 || height <= 0) {
	status_ignored = _cairo_surface_set_error (abstract_surface,
						   _cairo_error (CAIRO_STATUS_INVALID_SIZE));
	return;
    }

    surface = (cairo_xcb_surface_t *) abstract_surface;
    surface->width  = width;
    surface->height = height;
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_surface_set_size);
#endif
