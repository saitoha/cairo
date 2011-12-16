/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
 * Copyright © 2011 Intel Corporation
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 */

#include "cairoint.h"

#include "cairo-gl-private.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-surface-backend-private.h"
#include "cairo-surface-offset-private.h"
#include "cairo-surface-subsurface-inline.h"

static cairo_int_status_t
_cairo_gl_create_gradient_texture (cairo_gl_surface_t *dst,
				   const cairo_gradient_pattern_t *pattern,
                                   cairo_gl_gradient_t **gradient)
{
    cairo_gl_context_t *ctx;
    cairo_status_t status;

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    status = _cairo_gl_gradient_create (ctx, pattern->n_stops, pattern->stops, gradient);

    return _cairo_gl_context_release (ctx, status);
}

static cairo_int_status_t
_resolve_multisampling (cairo_gl_surface_t *surface)
{
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    if (! surface->msaa_active)
	return CAIRO_INT_STATUS_SUCCESS;

    if (surface->base.device == NULL)
	return CAIRO_INT_STATUS_SUCCESS;

    /* GLES surfaces do not need explicit resolution. */
    if (((cairo_gl_context_t *) surface->base.device)->gl_flavor == CAIRO_GL_FLAVOR_ES)
	return CAIRO_INT_STATUS_SUCCESS;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
	return status;

    ctx->current_target = surface;

#if CAIRO_HAS_GL_SURFACE
    _cairo_gl_activate_surface_as_nonmultisampling (ctx, surface);
#endif

    status = _cairo_gl_context_release (ctx, status);
    return status;
}

static cairo_status_t
_cairo_gl_subsurface_clone_operand_init (cairo_gl_operand_t *operand,
					 const cairo_pattern_t *_src,
					 cairo_gl_surface_t *dst,
					 const cairo_rectangle_int_t *sample,
					 const cairo_rectangle_int_t *extents)
{
    const cairo_surface_pattern_t *src = (cairo_surface_pattern_t *)_src;
    cairo_surface_pattern_t local_pattern;
    cairo_surface_subsurface_t *sub;
    cairo_gl_surface_t *surface;
    cairo_gl_context_t *ctx;
    cairo_surface_attributes_t *attributes;
    cairo_status_t status;

    sub = (cairo_surface_subsurface_t *) src->surface;

    if (sub->snapshot &&
	sub->snapshot->type == CAIRO_SURFACE_TYPE_GL &&
	sub->snapshot->device == dst->base.device)
    {
	surface = (cairo_gl_surface_t *)
	    cairo_surface_reference (sub->snapshot);
    }
    else
    {
	status = _cairo_gl_context_acquire (dst->base.device, &ctx);
	if (unlikely (status))
	    return status;

	/* XXX Trim surface to the sample area within the subsurface? */
	surface = (cairo_gl_surface_t *)
	    _cairo_gl_surface_create_scratch (ctx,
					      sub->target->content,
					      sub->extents.width,
					      sub->extents.height);
	if (surface->base.status)
	    return _cairo_gl_context_release (ctx, surface->base.status);

	_cairo_pattern_init_for_surface (&local_pattern, sub->target);
	cairo_matrix_init_translate (&local_pattern.base.matrix,
				     sub->extents.x, sub->extents.y);
	local_pattern.base.filter = CAIRO_FILTER_NEAREST;
	status = _cairo_surface_paint (&surface->base,
				       CAIRO_OPERATOR_SOURCE,
				       &local_pattern.base,
				       NULL);
	_cairo_pattern_fini (&local_pattern.base);

	status = _cairo_gl_context_release (ctx, status);
	if (unlikely (status)) {
	    cairo_surface_destroy (&surface->base);
	    return status;
	}

	_cairo_surface_subsurface_set_snapshot (&sub->base, &surface->base);
    }

    status = _resolve_multisampling (surface);
    if (unlikely (status))
        return status;

    attributes = &operand->texture.attributes;

    operand->type = CAIRO_GL_OPERAND_TEXTURE;
    operand->texture.surface = surface;
    operand->texture.owns_surface = surface;
    operand->texture.tex = surface->tex;

    if (_cairo_gl_device_requires_power_of_two_textures (dst->base.device)) {
	attributes->matrix = src->base.matrix;
    } else {
	cairo_matrix_t m;

	cairo_matrix_init_scale (&m,
				 1.0 / surface->width,
				 1.0 / surface->height);
	cairo_matrix_multiply (&attributes->matrix, &src->base.matrix, &m);
    }

    attributes->extend = src->base.extend;
    attributes->filter = src->base.filter;
    attributes->has_component_alpha = src->base.has_component_alpha;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_subsurface_operand_init (cairo_gl_operand_t *operand,
				   const cairo_pattern_t *_src,
				   cairo_gl_surface_t *dst,
				   const cairo_rectangle_int_t *sample,
				   const cairo_rectangle_int_t *extents)
{
    const cairo_surface_pattern_t *src = (cairo_surface_pattern_t *)_src;
    cairo_surface_subsurface_t *sub;
    cairo_gl_surface_t *surface;
    cairo_surface_attributes_t *attributes;
    cairo_int_status_t status;

    sub = (cairo_surface_subsurface_t *) src->surface;

    if (sample->x < 0 || sample->y < 0 ||
	sample->x + sample->width  > sub->extents.width ||
	sample->y + sample->height > sub->extents.height)
    {
	return _cairo_gl_subsurface_clone_operand_init (operand, _src,
							dst, sample, extents);
    }

    surface = (cairo_gl_surface_t *) sub->target;
    if (surface->base.device && surface->base.device != dst->base.device)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _resolve_multisampling (surface);
    if (unlikely (status))
	return status;

    /* Translate the matrix from
     * (unnormalized src -> unnormalized src) to
     * (unnormalized dst -> unnormalized src)
     */
    *operand = surface->operand;

    attributes = &operand->texture.attributes;
    attributes->matrix = src->base.matrix;
    attributes->matrix.x0 += sub->extents.x;
    attributes->matrix.y0 += sub->extents.y;
    cairo_matrix_multiply (&attributes->matrix,
			   &attributes->matrix,
			   &surface->operand.texture.attributes.matrix);

    attributes->extend = src->base.extend;
    attributes->filter = src->base.filter;
    attributes->has_component_alpha = src->base.has_component_alpha;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_operand_init (cairo_gl_operand_t *operand,
				const cairo_pattern_t *_src,
				cairo_gl_surface_t *dst,
				const cairo_rectangle_int_t *sample,
				const cairo_rectangle_int_t *extents)
{
    const cairo_surface_pattern_t *src = (cairo_surface_pattern_t *)_src;
    cairo_gl_surface_t *surface;
    cairo_surface_attributes_t *attributes;
    cairo_int_status_t status;

    surface = (cairo_gl_surface_t *) src->surface;
    if (surface->base.type != CAIRO_SURFACE_TYPE_GL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->base.backend->type != CAIRO_SURFACE_TYPE_GL) {
	if (_cairo_surface_is_subsurface (&surface->base))
	    return _cairo_gl_subsurface_operand_init (operand, _src, dst,
						      sample, extents);

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (surface->base.device && surface->base.device != dst->base.device)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _resolve_multisampling (surface);
    if (unlikely (status))
	return status;

    *operand = surface->operand;

    attributes = &operand->texture.attributes;
    cairo_matrix_multiply (&attributes->matrix,
			   &src->base.matrix,
			   &attributes->matrix);

    attributes->extend = src->base.extend;
    attributes->filter = src->base.filter;
    attributes->has_component_alpha = src->base.has_component_alpha;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_pattern_texture_setup (cairo_gl_operand_t *operand,
				 const cairo_pattern_t *_src,
				 cairo_gl_surface_t *dst,
				 const cairo_rectangle_int_t *extents)
{
    cairo_status_t status;
    cairo_gl_surface_t *surface;
    cairo_gl_context_t *ctx;
    cairo_surface_t *image;
    cairo_bool_t src_is_gl_surface = FALSE;

    if (_src->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_t* src_surface = ((cairo_surface_pattern_t *) _src)->surface;
	src_is_gl_surface = src_surface->type == CAIRO_SURFACE_TYPE_GL;
    }

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    surface = (cairo_gl_surface_t *)
	_cairo_gl_surface_create_scratch (ctx,
					  CAIRO_CONTENT_COLOR_ALPHA,
					  extents->width, extents->height);
    image = cairo_surface_map_to_image (&surface->base, NULL);

    /* If the pattern is a GL surface, it belongs to some other GL context,
       so we need to release this device while we paint it to the image. */
    if (src_is_gl_surface) {
	status = _cairo_gl_context_release (ctx, status);
	if (unlikely (status))
	    goto fail;
    }

    status = _cairo_surface_offset_paint (image, extents->x, extents->y,
					  CAIRO_OPERATOR_SOURCE, _src, NULL);

    if (src_is_gl_surface) {
	status = _cairo_gl_context_acquire (dst->base.device, &ctx);
	if (unlikely (status))
	    goto fail;
    }

    cairo_surface_unmap_image (&surface->base, image);
    status = _cairo_gl_context_release (ctx, status);
    if (unlikely (status))
	goto fail;

    *operand = surface->operand;
    operand->texture.owns_surface = surface;
    operand->texture.attributes.matrix.x0 -= extents->x * operand->texture.attributes.matrix.xx;
    operand->texture.attributes.matrix.y0 -= extents->y * operand->texture.attributes.matrix.yy;
    return CAIRO_STATUS_SUCCESS;

fail:
    cairo_surface_destroy (&surface->base);
    return status;
}

void
_cairo_gl_solid_operand_init (cairo_gl_operand_t *operand,
	                      const cairo_color_t *color)
{
    operand->type = CAIRO_GL_OPERAND_CONSTANT;
    operand->constant.color[0] = color->red   * color->alpha;
    operand->constant.color[1] = color->green * color->alpha;
    operand->constant.color[2] = color->blue  * color->alpha;
    operand->constant.color[3] = color->alpha;
}

void
_cairo_gl_operand_translate (cairo_gl_operand_t *operand,
			     double tx, double ty)
{
    operand->texture.attributes.matrix.x0 -= tx * operand->texture.attributes.matrix.xx;
    operand->texture.attributes.matrix.y0 -= ty * operand->texture.attributes.matrix.yy;
}

static cairo_status_t
_cairo_gl_gradient_operand_init (cairo_gl_operand_t *operand,
                                 const cairo_pattern_t *pattern,
				 cairo_gl_surface_t *dst)
{
    const cairo_gradient_pattern_t *gradient = (const cairo_gradient_pattern_t *)pattern;
    cairo_status_t status;

    assert (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR ||
	    gradient->base.type == CAIRO_PATTERN_TYPE_RADIAL);

    if (! _cairo_gl_device_has_glsl (dst->base.device))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_gl_create_gradient_texture (dst,
						gradient,
						&operand->gradient.gradient);
    if (unlikely (status))
	return status;

    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) gradient;
	double x0, y0, dx, dy, sf, offset;

	dx = linear->pd2.x - linear->pd1.x;
	dy = linear->pd2.y - linear->pd1.y;
	sf = 1.0 / (dx * dx + dy * dy);
	dx *= sf;
	dy *= sf;

	x0 = linear->pd1.x;
	y0 = linear->pd1.y;
	offset = dx * x0 + dy * y0;

	operand->type = CAIRO_GL_OPERAND_LINEAR_GRADIENT;

	cairo_matrix_init (&operand->gradient.m, dx, 0, dy, 1, -offset, 0);
	if (! _cairo_matrix_is_identity (&pattern->matrix)) {
	    cairo_matrix_multiply (&operand->gradient.m,
				   &pattern->matrix,
				   &operand->gradient.m);
	}
    } else {
	cairo_matrix_t m;
	cairo_circle_double_t circles[2];
	double x0, y0, r0, dx, dy, dr;

	/*
	 * Some fragment shader implementations use half-floats to
	 * represent numbers, so the maximum number they can represent
	 * is about 2^14. Some intermediate computations used in the
	 * radial gradient shaders can produce results of up to 2*k^4.
	 * Setting k=8 makes the maximum result about 8192 (assuming
	 * that the extreme circles are not much smaller than the
	 * destination image).
	 */
	_cairo_gradient_pattern_fit_to_range (gradient, 8.,
					      &operand->gradient.m, circles);

	x0 = circles[0].center.x;
	y0 = circles[0].center.y;
	r0 = circles[0].radius;
	dx = circles[1].center.x - x0;
	dy = circles[1].center.y - y0;
	dr = circles[1].radius   - r0;

	operand->gradient.a = dx * dx + dy * dy - dr * dr;
	operand->gradient.radius_0 = r0;
	operand->gradient.circle_d.center.x = dx;
	operand->gradient.circle_d.center.y = dy;
	operand->gradient.circle_d.radius   = dr;

	if (operand->gradient.a == 0)
	    operand->type = CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0;
	else if (pattern->extend == CAIRO_EXTEND_NONE)
	    operand->type = CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE;
	else
	    operand->type = CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT;

	cairo_matrix_init_translate (&m, -x0, -y0);
	cairo_matrix_multiply (&operand->gradient.m,
			       &operand->gradient.m,
			       &m);
    }

    operand->gradient.extend = pattern->extend;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_operand_copy (cairo_gl_operand_t *dst,
			const cairo_gl_operand_t *src)
{
    *dst = *src;
    switch (dst->type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	_cairo_gl_gradient_reference (dst->gradient.gradient);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	cairo_surface_reference (&dst->texture.owns_surface->base);
	break;
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
        break;
    }
}

void
_cairo_gl_operand_destroy (cairo_gl_operand_t *operand)
{
    switch (operand->type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	_cairo_gl_gradient_destroy (operand->gradient.gradient);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	cairo_surface_destroy (&operand->texture.owns_surface->base);
	break;
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
        break;
    }

    operand->type = CAIRO_GL_OPERAND_NONE;
}

cairo_int_status_t
_cairo_gl_operand_init (cairo_gl_operand_t *operand,
		        const cairo_pattern_t *pattern,
		        cairo_gl_surface_t *dst,
			const cairo_rectangle_int_t *sample,
			const cairo_rectangle_int_t *extents)
{
    cairo_int_status_t status;

    TRACE ((stderr, "%s: type=%d\n", __FUNCTION__, pattern->type));
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	_cairo_gl_solid_operand_init (operand,
				      &((cairo_solid_pattern_t *) pattern)->color);
	return CAIRO_STATUS_SUCCESS;
    case CAIRO_PATTERN_TYPE_SURFACE:
	status = _cairo_gl_surface_operand_init (operand, pattern, dst,
						 sample, extents);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    break;

	return status;

    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	status = _cairo_gl_gradient_operand_init (operand, pattern, dst);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    break;

	return status;

    default:
    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	break;
    }

    return _cairo_gl_pattern_texture_setup (operand, pattern, dst, extents);
}

cairo_filter_t
_cairo_gl_operand_get_filter (cairo_gl_operand_t *operand)
{
    cairo_filter_t filter;

    switch ((int) operand->type) {
    case CAIRO_GL_OPERAND_TEXTURE:
	filter = operand->texture.attributes.filter;
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	filter = CAIRO_FILTER_BILINEAR;
	break;
    default:
	filter = CAIRO_FILTER_DEFAULT;
	break;
    }

    return filter;
}

GLint
_cairo_gl_operand_get_gl_filter (cairo_gl_operand_t *operand)
{
    cairo_filter_t filter = _cairo_gl_operand_get_filter (operand);

    return filter != CAIRO_FILTER_FAST && filter != CAIRO_FILTER_NEAREST ?
	   GL_LINEAR :
	   GL_NEAREST;
}

cairo_extend_t
_cairo_gl_operand_get_extend (cairo_gl_operand_t *operand)
{
    cairo_extend_t extend;

    switch ((int) operand->type) {
    case CAIRO_GL_OPERAND_TEXTURE:
	extend = operand->texture.attributes.extend;
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	extend = operand->gradient.extend;
	break;
    default:
	extend = CAIRO_EXTEND_NONE;
	break;
    }

    return extend;
}


void
_cairo_gl_operand_bind_to_shader (cairo_gl_context_t *ctx,
                                  cairo_gl_operand_t *operand,
                                  cairo_gl_tex_t      tex_unit)
{
    char uniform_name[50];
    char *custom_part;
    static const char *names[] = { "source", "mask" };

    strcpy (uniform_name, names[tex_unit]);
    custom_part = uniform_name + strlen (names[tex_unit]);

    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
        break;
    case CAIRO_GL_OPERAND_CONSTANT:
        strcpy (custom_part, "_constant");
	_cairo_gl_shader_bind_vec4 (ctx,
                                    uniform_name,
                                    operand->constant.color[0],
                                    operand->constant.color[1],
                                    operand->constant.color[2],
                                    operand->constant.color[3]);
        break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	strcpy (custom_part, "_a");
	_cairo_gl_shader_bind_float  (ctx,
				      uniform_name,
				      operand->gradient.a);
	/* fall through */
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
	strcpy (custom_part, "_circle_d");
	_cairo_gl_shader_bind_vec3   (ctx,
				      uniform_name,
				      operand->gradient.circle_d.center.x,
				      operand->gradient.circle_d.center.y,
				      operand->gradient.circle_d.radius);
	strcpy (custom_part, "_radius_0");
	_cairo_gl_shader_bind_float  (ctx,
				      uniform_name,
				      operand->gradient.radius_0);
        /* fall through */
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_TEXTURE:
	/*
	 * For GLES2 we use shaders to implement GL_CLAMP_TO_BORDER (used
	 * with CAIRO_EXTEND_NONE). When bilinear filtering is enabled,
	 * these shaders need the texture dimensions for their calculations.
	 */
	if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES &&
	    _cairo_gl_operand_get_extend (operand) == CAIRO_EXTEND_NONE &&
	    _cairo_gl_operand_get_gl_filter (operand) == GL_LINEAR)
	{
	    float width, height;
	    if (operand->type == CAIRO_GL_OPERAND_TEXTURE) {
		width = operand->texture.surface->width;
		height = operand->texture.surface->height;
	    }
	    else {
		width = operand->gradient.gradient->cache_entry.size,
		height = 1;
	    }
	    strcpy (custom_part, "_texdims");
	    _cairo_gl_shader_bind_vec2 (ctx, uniform_name, width, height);
	}
        break;
    }
}


cairo_bool_t
_cairo_gl_operand_needs_setup (cairo_gl_operand_t *dest,
                               cairo_gl_operand_t *source,
                               unsigned int        vertex_offset)
{
    if (dest->type != source->type)
        return TRUE;
    if (dest->vertex_offset != vertex_offset)
        return TRUE;

    switch (source->type) {
    case CAIRO_GL_OPERAND_NONE:
        return FALSE;
    case CAIRO_GL_OPERAND_CONSTANT:
        return dest->constant.color[0] != source->constant.color[0] ||
               dest->constant.color[1] != source->constant.color[1] ||
               dest->constant.color[2] != source->constant.color[2] ||
               dest->constant.color[3] != source->constant.color[3];
    case CAIRO_GL_OPERAND_TEXTURE:
        return dest->texture.surface != source->texture.surface ||
               dest->texture.attributes.extend != source->texture.attributes.extend ||
               dest->texture.attributes.filter != source->texture.attributes.filter ||
               dest->texture.attributes.has_component_alpha != source->texture.attributes.has_component_alpha;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
        /* XXX: improve this */
        return TRUE;
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
        break;
    }
    return TRUE;
}

unsigned int
_cairo_gl_operand_get_vertex_size (cairo_gl_operand_type_t type)
{
    switch (type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
        return 0;
    case CAIRO_GL_OPERAND_TEXTURE:
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
        return 2 * sizeof (GLfloat);
    }
}

void
_cairo_gl_operand_emit (cairo_gl_operand_t *operand,
                        GLfloat ** vb,
                        GLfloat x,
                        GLfloat y)
{
    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
        break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
        {
	    double s = x;
	    double t = y;

	    cairo_matrix_transform_point (&operand->gradient.m, &s, &t);

	    *(*vb)++ = s;
	    *(*vb)++ = t;
        }
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
        {
            cairo_surface_attributes_t *src_attributes = &operand->texture.attributes;
            double s = x;
            double t = y;

            cairo_matrix_transform_point (&src_attributes->matrix, &s, &t);
            *(*vb)++ = s;
            *(*vb)++ = t;
        }
        break;
    }
}
