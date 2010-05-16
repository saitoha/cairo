/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005 Red Hat, Inc
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
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-gl-private.h"

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void			   *abstract_surface,
				   cairo_operator_t	    op,
				   const cairo_color_t     *color,
				   cairo_rectangle_int_t   *rects,
				   int			    num_rects);

#define BIAS .375

static cairo_bool_t _cairo_surface_is_gl (cairo_surface_t *surface)
{
    return surface->backend == &_cairo_gl_surface_backend;
}

cairo_bool_t
_cairo_gl_get_image_format_and_type (pixman_format_code_t pixman_format,
				     GLenum *internal_format, GLenum *format,
				     GLenum *type, cairo_bool_t *has_alpha)
{
    *has_alpha = TRUE;

    switch (pixman_format) {
    case PIXMAN_a8r8g8b8:
	*internal_format = GL_RGBA;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	return TRUE;
    case PIXMAN_x8r8g8b8:
	*internal_format = GL_RGB;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_a8b8g8r8:
	*internal_format = GL_RGBA;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	return TRUE;
    case PIXMAN_x8b8g8r8:
	*internal_format = GL_RGB;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_b8g8r8a8:
	*internal_format = GL_RGBA;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8;
	return TRUE;
    case PIXMAN_b8g8r8x8:
	*internal_format = GL_RGB;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_r8g8b8:
	*internal_format = GL_RGB;
	*format = GL_RGB;
	*type = GL_UNSIGNED_BYTE;
	return TRUE;
    case PIXMAN_b8g8r8:
	*internal_format = GL_RGB;
	*format = GL_BGR;
	*type = GL_UNSIGNED_BYTE;
	return TRUE;
    case PIXMAN_r5g6b5:
	*internal_format = GL_RGB;
	*format = GL_RGB;
	*type = GL_UNSIGNED_SHORT_5_6_5;
	return TRUE;
    case PIXMAN_b5g6r5:
	*internal_format = GL_RGB;
	*format = GL_RGB;
	*type = GL_UNSIGNED_SHORT_5_6_5_REV;
	return TRUE;
    case PIXMAN_a1r5g5b5:
	*internal_format = GL_RGBA;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	return TRUE;
    case PIXMAN_x1r5g5b5:
	*internal_format = GL_RGB;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_a1b5g5r5:
	*internal_format = GL_RGBA;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	return TRUE;
    case PIXMAN_x1b5g5r5:
	*internal_format = GL_RGB;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_a8:
	*internal_format = GL_ALPHA;
	*format = GL_ALPHA;
	*type = GL_UNSIGNED_BYTE;
	return TRUE;

    case PIXMAN_a2b10g10r10:
    case PIXMAN_x2b10g10r10:
    case PIXMAN_a4r4g4b4:
    case PIXMAN_x4r4g4b4:
    case PIXMAN_a4b4g4r4:
    case PIXMAN_x4b4g4r4:
    case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:
    case PIXMAN_a2r2g2b2:
    case PIXMAN_a2b2g2r2:
    case PIXMAN_c8:
    case PIXMAN_x4a4:
    /* case PIXMAN_x4c4: */
    case PIXMAN_x4g4:
    case PIXMAN_a4:
    case PIXMAN_r1g2b1:
    case PIXMAN_b1g2r1:
    case PIXMAN_a1r1g1b1:
    case PIXMAN_a1b1g1r1:
    case PIXMAN_c4:
    case PIXMAN_g4:
    case PIXMAN_a1:
    case PIXMAN_g1:
    case PIXMAN_yuy2:
    case PIXMAN_yv12:
    case PIXMAN_x2r10g10b10:
    case PIXMAN_a2r10g10b10:
    default:
	return FALSE;
    }
}

cairo_bool_t
_cairo_gl_operator_is_supported (cairo_operator_t op)
{
    return op < CAIRO_OPERATOR_SATURATE;
}

void
_cairo_gl_set_operator (cairo_gl_surface_t *dst, cairo_operator_t op,
			cairo_bool_t component_alpha)
{
    struct {
	GLenum src;
	GLenum dst;
    } blend_factors[] = {
	{ GL_ZERO, GL_ZERO }, /* Clear */
	{ GL_ONE, GL_ZERO }, /* Source */
	{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA }, /* Over */
	{ GL_DST_ALPHA, GL_ZERO }, /* In */
	{ GL_ONE_MINUS_DST_ALPHA, GL_ZERO }, /* Out */
	{ GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, /* Atop */

	{ GL_ZERO, GL_ONE }, /* Dest */
	{ GL_ONE_MINUS_DST_ALPHA, GL_ONE }, /* DestOver */
	{ GL_ZERO, GL_SRC_ALPHA }, /* DestIn */
	{ GL_ZERO, GL_ONE_MINUS_SRC_ALPHA }, /* DestOut */
	{ GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA }, /* DestAtop */

	{ GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, /* Xor */
	{ GL_ONE, GL_ONE }, /* Add */
    };
    GLenum src_factor, dst_factor;

    assert (op < ARRAY_LENGTH (blend_factors));

    src_factor = blend_factors[op].src;
    dst_factor = blend_factors[op].dst;

    /* Even when the user requests CAIRO_CONTENT_COLOR, we use GL_RGBA
     * due to texture filtering of GL_CLAMP_TO_BORDER.  So fix those
     * bits in that case.
     */
    if (dst->base.content == CAIRO_CONTENT_COLOR) {
	if (src_factor == GL_ONE_MINUS_DST_ALPHA)
	    src_factor = GL_ZERO;
	if (src_factor == GL_DST_ALPHA)
	    src_factor = GL_ONE;
    }

    if (component_alpha) {
	if (dst_factor == GL_ONE_MINUS_SRC_ALPHA)
	    dst_factor = GL_ONE_MINUS_SRC_COLOR;
	if (dst_factor == GL_SRC_ALPHA)
	    dst_factor = GL_SRC_COLOR;
    }

    glEnable (GL_BLEND);
    if (dst->base.content == CAIRO_CONTENT_ALPHA) {
        glBlendFuncSeparate (GL_ZERO, GL_ZERO, src_factor, dst_factor);
    } else {
        glBlendFunc (src_factor, dst_factor);
    }
}

void
_cairo_gl_surface_init (cairo_device_t *device,
			cairo_gl_surface_t *surface,
			cairo_content_t content,
			int width, int height)
{
    _cairo_surface_init (&surface->base,
			 &_cairo_gl_surface_backend,
			 device,
			 content);

    surface->width = width;
    surface->height = height;
}

static cairo_surface_t *
_cairo_gl_surface_create_scratch (cairo_gl_context_t   *ctx,
				  cairo_content_t	content,
				  int			width,
				  int			height)
{
    cairo_gl_surface_t *surface;
    GLenum format;
    cairo_status_t status;

    assert (width <= ctx->max_framebuffer_size && height <= ctx->max_framebuffer_size);

    surface = calloc (1, sizeof (cairo_gl_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_gl_surface_init (&ctx->base, surface, content, width, height);

    /* adjust the texture size after setting our real extents */
    if (width < 1)
	width = 1;
    if (height < 1)
	height = 1;

    switch (content) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = GL_RGBA;
	break;
    case CAIRO_CONTENT_ALPHA:
	/* We want to be trying GL_ALPHA framebuffer objects here. */
	format = GL_RGBA;
	break;
    case CAIRO_CONTENT_COLOR:
	/* GL_RGB is almost what we want here -- sampling 1 alpha when
	 * texturing, using 1 as destination alpha factor in blending,
	 * etc.  However, when filtering with GL_CLAMP_TO_BORDER, the
	 * alpha channel of the border color will also be clamped to
	 * 1, when we actually want the border color we explicitly
	 * specified.  So, we have to store RGBA, and fill the alpha
	 * channel with 1 when blending.
	 */
	format = GL_RGBA;
	break;
    }

    /* Create the texture used to store the surface's data. */
    glGenTextures (1, &surface->tex);
    glBindTexture (ctx->tex_target, surface->tex);
    glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D (ctx->tex_target, 0, format, width, height, 0,
		  format, GL_UNSIGNED_BYTE, NULL);

    /* Create a framebuffer object wrapping the texture so that we can render
     * to it.
     */
    glGenFramebuffersEXT (1, &surface->fb);
    glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, surface->fb);
    glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			       GL_COLOR_ATTACHMENT0_EXT,
			       ctx->tex_target,
			       surface->tex,
			       0);
    ctx->current_target = NULL;

    status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
	fprintf (stderr, "destination is framebuffer incomplete\n");

    return &surface->base;
}

cairo_status_t
_cairo_gl_surface_clear (cairo_gl_surface_t *surface)
{
    cairo_gl_context_t *ctx;
    cairo_status_t status;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
	return status;

    _cairo_gl_context_set_destination (ctx, surface);
    if (surface->base.content == CAIRO_CONTENT_COLOR)
	glClearColor (0.0, 0.0, 0.0, 1.0);
    else
	glClearColor (0.0, 0.0, 0.0, 0.0);
    glClear (GL_COLOR_BUFFER_BIT);
    _cairo_gl_context_release (ctx);

    surface->base.is_clear = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *
cairo_gl_surface_create (cairo_device_t		*abstract_device,
			 cairo_content_t	 content,
			 int			 width,
			 int			 height)
{
    cairo_gl_context_t *ctx;
    cairo_gl_surface_t *surface;
    cairo_status_t status;

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    if (abstract_device == NULL) {
	return cairo_image_surface_create (_cairo_format_from_content (content),
					   width, height);
    }

    if (abstract_device->backend->type != CAIRO_DEVICE_TYPE_GL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    status = _cairo_gl_context_acquire (abstract_device, &ctx);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    surface = (cairo_gl_surface_t *)
	_cairo_gl_surface_create_scratch (ctx, content, width, height);
    if (unlikely (surface->base.status)) {
	_cairo_gl_context_release (ctx);
	return &surface->base;
    }

    /* Cairo surfaces start out initialized to transparent (black) */
    status = _cairo_gl_surface_clear (surface);
    if (unlikely (status)) {
	cairo_surface_destroy (&surface->base);
	_cairo_gl_context_release (ctx);
	return _cairo_surface_create_in_error (status);
    }

    _cairo_gl_context_release (ctx);

    return &surface->base;
}
slim_hidden_def (cairo_gl_surface_create);

void
cairo_gl_surface_set_size (cairo_surface_t *abstract_surface,
			   int              width,
			   int              height)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;

    if (! _cairo_surface_is_gl (abstract_surface) || surface->fb) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    surface->width = width;
    surface->height = height;
}

int
cairo_gl_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;

    if (! _cairo_surface_is_gl (abstract_surface))
	return 0;

    return surface->width;
}

int
cairo_gl_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;

    if (! _cairo_surface_is_gl (abstract_surface))
	return 0;

    return surface->height;
}

void
cairo_gl_surface_swapbuffers (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;

    if (! _cairo_surface_is_gl (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (! surface->fb) {
	cairo_gl_context_t *ctx;
        
        if (_cairo_gl_context_acquire (surface->base.device, &ctx))
            return;

	ctx->swap_buffers (ctx, surface);

        _cairo_gl_context_release (ctx);
    }
}

static cairo_surface_t *
_cairo_gl_surface_create_similar (void		 *abstract_surface,
				  cairo_content_t  content,
				  int		  width,
				  int		  height)
{
    cairo_surface_t *surface = abstract_surface;
    cairo_gl_context_t *ctx;
    cairo_status_t status;

    if (width < 1 || height < 1)
        return cairo_image_surface_create (_cairo_format_from_content (content),
                                           width, height);

    status = _cairo_gl_context_acquire (surface->device, &ctx);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    if (width > ctx->max_framebuffer_size ||
	height > ctx->max_framebuffer_size)
    {
	surface = NULL;
        goto RELEASE;
    }

    surface = _cairo_gl_surface_create_scratch (ctx, content, width, height);

RELEASE:
    _cairo_gl_context_release (ctx);

    return surface;
}

cairo_status_t
_cairo_gl_surface_draw_image (cairo_gl_surface_t *dst,
			      cairo_image_surface_t *src,
			      int src_x, int src_y,
			      int width, int height,
			      int dst_x, int dst_y)
{
    GLenum internal_format, format, type;
    cairo_bool_t has_alpha;
    cairo_image_surface_t *clone = NULL;
    cairo_gl_context_t *ctx;
    int cpp;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (! _cairo_gl_get_image_format_and_type (src->pixman_format,
					       &internal_format,
					       &format,
					       &type,
					       &has_alpha))
    {
	cairo_bool_t is_supported;

	clone = _cairo_image_surface_coerce (src);
	if (unlikely (clone->base.status))
	    return clone->base.status;

	is_supported =
	    _cairo_gl_get_image_format_and_type (clone->pixman_format,
		                                 &internal_format,
						 &format,
						 &type,
						 &has_alpha);
	assert (is_supported);
	src = clone;
    }

    cpp = PIXMAN_FORMAT_BPP (src->pixman_format) / 8;

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, src->stride / cpp);
    if (dst->fb) {
	glBindTexture (ctx->tex_target, dst->tex);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexSubImage2D (ctx->tex_target, 0,
			 dst_x, dst_y, width, height,
			 format, type,
			 src->data + src_y * src->stride + src_x * cpp);

	/* If we just treated some rgb-only data as rgba, then we have to
	 * go back and fix up the alpha channel where we filled in this
	 * texture data.
	 */
	if (!has_alpha) {
	    cairo_rectangle_int_t rect;
	    cairo_color_t color;

	    rect.x = dst_x;
	    rect.y = dst_y;
	    rect.width = width;
	    rect.height = height;

	    color.red = 0.0;
	    color.green = 0.0;
	    color.blue = 0.0;
	    color.alpha = 1.0;

	    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	    _cairo_gl_surface_fill_rectangles (dst,
					       CAIRO_OPERATOR_SOURCE,
					       &color,
					       &rect, 1);
	    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
    } else {
	GLuint tex;
	GLfloat vertices[8], texcoords[8];

	if (_cairo_gl_device_has_glsl (&ctx->base)) {
	    cairo_gl_shader_program_t *program;

	    status = _cairo_gl_get_program (ctx,
					    CAIRO_GL_OPERAND_TEXTURE,
					    CAIRO_GL_OPERAND_NONE,
					    CAIRO_GL_SHADER_IN_NORMAL,
					    &program);
	    if (_cairo_status_is_error (status)) {
		_cairo_gl_context_release (ctx);
		goto fail;
	    }

	    _cairo_gl_use_program (ctx, program);
	} else {
	    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}

	status = CAIRO_STATUS_SUCCESS;

	_cairo_gl_context_set_destination (ctx, dst);

	glGenTextures (1, &tex);
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (ctx->tex_target, tex);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D (ctx->tex_target, 0, internal_format, width, height, 0,
		      format, type, src->data + src_y * src->stride + src_x * cpp);

	glEnable (ctx->tex_target);
	glDisable (GL_BLEND);

	vertices[0] = dst_x;
	vertices[1] = dst_y;
	vertices[2] = dst_x + width;
	vertices[3] = dst_y;
	vertices[4] = dst_x + width;
	vertices[5] = dst_y + height;
	vertices[6] = dst_x;
	vertices[7] = dst_y + height;

	if (ctx->tex_target != GL_TEXTURE_RECTANGLE_EXT) {
	    texcoords[0] = 0;
	    texcoords[1] = 0;
	    texcoords[2] = 1;
	    texcoords[3] = 0;
	    texcoords[4] = 1;
	    texcoords[5] = 1;
	    texcoords[6] = 0;
	    texcoords[7] = 1;
	} else {
	    texcoords[0] = 0;
	    texcoords[1] = 0;
	    texcoords[2] = width;
	    texcoords[3] = 0;
	    texcoords[4] = width;
	    texcoords[5] = height;
	    texcoords[6] = 0;
	    texcoords[7] = height;
	}

        glVertexPointer (2, GL_FLOAT, sizeof (GLfloat) * 2, vertices);
	glEnableClientState (GL_VERTEX_ARRAY);

	glClientActiveTexture (GL_TEXTURE0);
	glTexCoordPointer (2, GL_FLOAT, sizeof (GLfloat) * 2, texcoords);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);

	glDrawArrays (GL_QUADS, 0, 4);

	glDisableClientState (GL_COLOR_ARRAY);
	glDisableClientState (GL_VERTEX_ARRAY);
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);

	if (_cairo_gl_device_has_glsl (&ctx->base))
	    _cairo_gl_use_program (ctx, NULL);
	glDeleteTextures (1, &tex);
	glDisable (ctx->tex_target);
    }

fail:
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

    _cairo_gl_context_release (ctx);

    if (clone)
        cairo_surface_destroy (&clone->base);

    return status;
}

static cairo_status_t
_cairo_gl_surface_get_image (cairo_gl_surface_t      *surface,
			     cairo_rectangle_int_t   *interest,
			     cairo_image_surface_t  **image_out,
			     cairo_rectangle_int_t   *rect_out)
{
    cairo_image_surface_t *image;
    cairo_gl_context_t *ctx;
    GLenum format, type;
    cairo_format_t cairo_format;
    unsigned int cpp;
    cairo_status_t status;

    /* Want to use a switch statement here but the compiler gets whiny. */
    if (surface->base.content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_ARGB32;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->base.content == CAIRO_CONTENT_COLOR) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_RGB24;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->base.content == CAIRO_CONTENT_ALPHA) {
	format = GL_ALPHA;
	cairo_format = CAIRO_FORMAT_A8;
	type = GL_UNSIGNED_BYTE;
	cpp = 1;
    } else {
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    image = (cairo_image_surface_t*)
	cairo_image_surface_create (cairo_format,
				    interest->width, interest->height);
    if (unlikely (image->base.status))
	return image->base.status;

    /* This is inefficient, as we'd rather just read the thing without making
     * it the destination.  But then, this is the fallback path, so let's not
     * fall back instead.
     */
    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
        return status;
    _cairo_gl_context_set_destination (ctx, surface);

    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glPixelStorei (GL_PACK_ROW_LENGTH, image->stride / cpp);
    if (surface->fb == 0 && GLEW_MESA_pack_invert)
	glPixelStorei (GL_PACK_INVERT_MESA, 1);
    glReadPixels (interest->x, interest->y,
		  interest->width, interest->height,
		  format, type, image->data);
    if (surface->fb == 0 && GLEW_MESA_pack_invert)
	glPixelStorei (GL_PACK_INVERT_MESA, 0);

    _cairo_gl_context_release (ctx);

    *image_out = image;
    if (rect_out != NULL)
	*rect_out = *interest;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_finish (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_status_t status;
    cairo_gl_context_t *ctx;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
        return status;

    glDeleteFramebuffersEXT (1, &surface->fb);
    glDeleteTextures (1, &surface->tex);

    if (ctx->current_target == surface)
	ctx->current_target = NULL;

    _cairo_gl_context_release (ctx);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_acquire_source_image (void		       *abstract_surface,
					cairo_image_surface_t **image_out,
					void		      **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_rectangle_int_t extents;

    *image_extra = NULL;

    extents.x = extents.y = 0;
    extents.width = surface->width;
    extents.height = surface->height;
    return _cairo_gl_surface_get_image (surface, &extents, image_out, NULL);
}

static void
_cairo_gl_surface_release_source_image (void		      *abstract_surface,
					cairo_image_surface_t *image,
					void		      *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_gl_surface_acquire_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t  **image_out,
				      cairo_rectangle_int_t   *image_rect_out,
				      void		     **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    *image_extra = NULL;
    return _cairo_gl_surface_get_image (surface, interest_rect, image_out,
					image_rect_out);
}

static void
_cairo_gl_surface_release_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t   *image,
				      cairo_rectangle_int_t   *image_rect,
				      void		      *image_extra)
{
    cairo_status_t status;

    status = _cairo_gl_surface_draw_image (abstract_surface, image,
					   0, 0,
					   image->width, image->height,
					   image_rect->x, image_rect->y);
    /* as we created the image, its format should be directly applicable */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_gl_surface_clone_similar (void		     *abstract_surface,
				 cairo_surface_t     *src,
				 int                  src_x,
				 int                  src_y,
				 int                  width,
				 int                  height,
				 int                 *clone_offset_x,
				 int                 *clone_offset_y,
				 cairo_surface_t    **clone_out)
{
    cairo_gl_surface_t *surface = abstract_surface;

    if (src->device == surface->base.device) {
	*clone_offset_x = 0;
	*clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_gl_surface_t *clone;
	cairo_status_t status;

	clone = (cairo_gl_surface_t *)
	    _cairo_gl_surface_create_similar (&surface->base,
		                              src->content,
					      width, height);
	if (clone == NULL)
	    return UNSUPPORTED ("create_similar failed");
	if (clone->base.status)
	    return clone->base.status;

	status = _cairo_gl_surface_draw_image (clone, image_src,
					       src_x, src_y,
					       width, height,
					       0, 0);
	if (status) {
	    cairo_surface_destroy (&clone->base);
	    return status;
	}

	*clone_out = &clone->base;
	*clone_offset_x = src_x;
	*clone_offset_y = src_y;

	return CAIRO_STATUS_SUCCESS;
    }

    return UNSUPPORTED ("unknown src surface type in clone_similar");
}

/** Creates a cairo-gl pattern surface for the given trapezoids */
static cairo_status_t
_cairo_gl_get_traps_pattern (cairo_gl_surface_t *dst,
			     int dst_x, int dst_y,
			     int width, int height,
			     cairo_trapezoid_t *traps,
			     int num_traps,
			     cairo_antialias_t antialias,
			     cairo_surface_pattern_t *pattern)
{
    pixman_format_code_t pixman_format;
    pixman_image_t *image;
    cairo_surface_t *surface;
    int i;

    pixman_format = antialias != CAIRO_ANTIALIAS_NONE ? PIXMAN_a8 : PIXMAN_a1,
    image = pixman_image_create_bits (pixman_format, width, height, NULL, 0);
    if (unlikely (image == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    for (i = 0; i < num_traps; i++) {
	pixman_trapezoid_t trap;

	trap.top = _cairo_fixed_to_16_16 (traps[i].top);
	trap.bottom = _cairo_fixed_to_16_16 (traps[i].bottom);

	trap.left.p1.x = _cairo_fixed_to_16_16 (traps[i].left.p1.x);
	trap.left.p1.y = _cairo_fixed_to_16_16 (traps[i].left.p1.y);
	trap.left.p2.x = _cairo_fixed_to_16_16 (traps[i].left.p2.x);
	trap.left.p2.y = _cairo_fixed_to_16_16 (traps[i].left.p2.y);

	trap.right.p1.x = _cairo_fixed_to_16_16 (traps[i].right.p1.x);
	trap.right.p1.y = _cairo_fixed_to_16_16 (traps[i].right.p1.y);
	trap.right.p2.x = _cairo_fixed_to_16_16 (traps[i].right.p2.x);
	trap.right.p2.y = _cairo_fixed_to_16_16 (traps[i].right.p2.y);

	pixman_rasterize_trapezoid (image, &trap, -dst_x, -dst_y);
    }

    surface = _cairo_image_surface_create_for_pixman_image (image,
							    pixman_format);
    if (unlikely (surface->status)) {
	pixman_image_unref (image);
	return surface->status;
    }

    _cairo_pattern_init_for_surface (pattern, surface);
    cairo_surface_destroy (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_composite (cairo_operator_t		  op,
			     const cairo_pattern_t	 *src,
			     const cairo_pattern_t	 *mask,
			     void			 *abstract_dst,
			     int			  src_x,
			     int			  src_y,
			     int			  mask_x,
			     int			  mask_y,
			     int			  dst_x,
			     int			  dst_y,
			     unsigned int		  width,
			     unsigned int		  height,
			     cairo_region_t		 *clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_context_t *ctx;
    cairo_status_t status;
    cairo_gl_composite_t setup;
    cairo_rectangle_int_t rect = { dst_x, dst_y, width, height };

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    status = _cairo_gl_composite_init (ctx, &setup,
                                       op, dst, src, mask,
                                       mask && mask->has_component_alpha,
                                       &rect);
    if (unlikely (status))
        goto CLEANUP;

    status = _cairo_gl_operand_init (ctx, &setup.src, src, dst,
				     src_x, src_y,
				     dst_x, dst_y,
				     width, height);
    if (unlikely (status))
        goto CLEANUP;

    if (mask != NULL) {
	status = _cairo_gl_operand_init (ctx, &setup.mask, mask, dst,
					 mask_x, mask_y,
					 dst_x, dst_y,
					 width, height);
	if (unlikely (status))
            goto CLEANUP;
    }

    status = _cairo_gl_composite_begin (ctx, &setup);

    if (clip_region != NULL) {
        int i, num_rectangles;

        num_rectangles = cairo_region_num_rectangles (clip_region);

	for (i = 0; i < num_rectangles; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, i, &rect);
            _cairo_gl_composite_emit_rect (ctx, &setup,
                                           rect.x,              rect.y,
                                           rect.x + rect.width, rect.y + rect.height,
                                           0);
	}
    } else {
        _cairo_gl_composite_emit_rect (ctx, &setup,
                                       dst_x,         dst_y,
                                       dst_x + width, dst_y + height,
                                       0);
    }

    _cairo_gl_composite_end (ctx, &setup);

  CLEANUP:
    _cairo_gl_composite_fini (ctx, &setup);
    _cairo_gl_context_release (ctx);

    return status;
}

static cairo_int_status_t
_cairo_gl_surface_composite_trapezoids (cairo_operator_t op,
					const cairo_pattern_t *pattern,
					void *abstract_dst,
					cairo_antialias_t antialias,
					int src_x, int src_y,
					int dst_x, int dst_y,
					unsigned int width,
					unsigned int height,
					cairo_trapezoid_t *traps,
					int num_traps,
					cairo_region_t *clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_surface_pattern_t traps_pattern;
    cairo_int_status_t status;

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    if (_cairo_surface_check_span_renderer (op,pattern,&dst->base, antialias)) {
	status =
	    _cairo_surface_composite_trapezoids_as_polygon (&dst->base,
							    op, pattern,
							    antialias,
							    src_x, src_y,
							    dst_x, dst_y,
							    width, height,
							    traps, num_traps,
							    clip_region);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    status = _cairo_gl_get_traps_pattern (dst,
					  dst_x, dst_y, width, height,
					  traps, num_traps, antialias,
					  &traps_pattern);
    if (unlikely (status))
	return status;

    status = _cairo_gl_surface_composite (op,
					  pattern, &traps_pattern.base, dst,
					  src_x, src_y,
					  0, 0,
					  dst_x, dst_y,
					  width, height,
					  clip_region);

    _cairo_pattern_fini (&traps_pattern.base);

    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
    return status;
}

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles_fixed (void			 *abstract_surface,
					 cairo_operator_t	  op,
					 const cairo_color_t     *color,
					 cairo_rectangle_int_t   *rects,
					 int			  num_rects)
{
#define N_STACK_RECTS 4
    cairo_gl_surface_t *surface = abstract_surface;
    GLfloat vertices_stack[N_STACK_RECTS*4*2];
    GLfloat colors_stack[N_STACK_RECTS*4*4];
    cairo_gl_context_t *ctx;
    int i;
    GLfloat *vertices;
    GLfloat *colors;
    cairo_status_t status;

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
	return status;

    _cairo_gl_context_set_destination (ctx, surface);
    _cairo_gl_set_operator (surface, op, FALSE);

    if (num_rects > N_STACK_RECTS) {
	vertices = _cairo_malloc_ab (num_rects, sizeof (GLfloat) * 4 * 2);
	colors = _cairo_malloc_ab (num_rects, sizeof (GLfloat) * 4 * 4);
	if (!vertices || !colors) {
	    _cairo_gl_context_release (ctx);
	    free (vertices);
	    free (colors);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}
    } else {
	vertices = vertices_stack;
	colors = colors_stack;
    }

    /* This should be loaded in as either a blend constant and an operator
     * setup specific to this, or better, a fragment shader constant.
     */
    colors[0] = color->red * color->alpha;
    colors[1] = color->green * color->alpha;
    colors[2] = color->blue * color->alpha;
    colors[3] = color->alpha;
    for (i = 1; i < num_rects * 4; i++) {
	colors[i*4 + 0] = colors[0];
	colors[i*4 + 1] = colors[1];
	colors[i*4 + 2] = colors[2];
	colors[i*4 + 3] = colors[3];
    }

    for (i = 0; i < num_rects; i++) {
	vertices[i * 8 + 0] = rects[i].x;
	vertices[i * 8 + 1] = rects[i].y;
	vertices[i * 8 + 2] = rects[i].x + rects[i].width;
	vertices[i * 8 + 3] = rects[i].y;
	vertices[i * 8 + 4] = rects[i].x + rects[i].width;
	vertices[i * 8 + 5] = rects[i].y + rects[i].height;
	vertices[i * 8 + 6] = rects[i].x;
	vertices[i * 8 + 7] = rects[i].y + rects[i].height;
    }

    glVertexPointer (2, GL_FLOAT, sizeof (GLfloat)*2, vertices);
    glEnableClientState (GL_VERTEX_ARRAY);
    glColorPointer (4, GL_FLOAT, sizeof (GLfloat)*4, colors);
    glEnableClientState (GL_COLOR_ARRAY);

    glDrawArrays (GL_QUADS, 0, 4 * num_rects);

    glDisableClientState (GL_COLOR_ARRAY);
    glDisableClientState (GL_VERTEX_ARRAY);
    glDisable (GL_BLEND);

    _cairo_gl_context_release (ctx);
    if (vertices != vertices_stack)
	free (vertices);
    if (colors != colors_stack)
	free (colors);

    return CAIRO_STATUS_SUCCESS;
#undef N_STACK_RECTS
}

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles_glsl (void                  *abstract_surface,
					cairo_operator_t       op,
					const cairo_color_t   *color,
					cairo_rectangle_int_t *rects,
					int                    num_rects)
{
#define N_STACK_RECTS 4
    cairo_gl_surface_t *surface = abstract_surface;
    GLfloat vertices_stack[N_STACK_RECTS*4*2];
    cairo_gl_context_t *ctx;
    int i;
    GLfloat *vertices;
    static const char *fill_fs_source =
	"uniform vec4 color;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = color;\n"
	"}\n";
    cairo_status_t status;

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
	return status;

    status = create_shader_program (ctx,
                                    &ctx->fill_rectangles_shader,
                                    CAIRO_GL_VAR_NONE,
                                    CAIRO_GL_VAR_NONE,
				    fill_fs_source);
    if (unlikely (status)) {
	_cairo_gl_context_release (ctx);
	return status;
    }

    if (num_rects > N_STACK_RECTS) {
	vertices = _cairo_malloc_ab (num_rects, sizeof (GLfloat) * 4 * 2);
	if (!vertices) {
	    _cairo_gl_context_release (ctx);
	    free (vertices);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}
    } else {
	vertices = vertices_stack;
    }

    _cairo_gl_use_program (ctx, &ctx->fill_rectangles_shader);

    _cairo_gl_context_set_destination (ctx, surface);
    _cairo_gl_set_operator (surface, op, FALSE);

    status = bind_vec4_to_shader (ctx,
                                  ctx->fill_rectangles_shader.program,
                                  "color",
                                  color->red * color->alpha,
                                  color->green * color->alpha,
                                  color->blue * color->alpha,
                                  color->alpha);
    assert (! _cairo_status_is_error (status));

    for (i = 0; i < num_rects; i++) {
	vertices[i * 8 + 0] = rects[i].x;
	vertices[i * 8 + 1] = rects[i].y;
	vertices[i * 8 + 2] = rects[i].x + rects[i].width;
	vertices[i * 8 + 3] = rects[i].y;
	vertices[i * 8 + 4] = rects[i].x + rects[i].width;
	vertices[i * 8 + 5] = rects[i].y + rects[i].height;
	vertices[i * 8 + 6] = rects[i].x;
	vertices[i * 8 + 7] = rects[i].y + rects[i].height;
    }

    glVertexPointer (2, GL_FLOAT, sizeof (GLfloat)*2, vertices);
    glEnableClientState (GL_VERTEX_ARRAY);

    glDrawArrays (GL_QUADS, 0, 4 * num_rects);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisable (GL_BLEND);
    glUseProgramObjectARB (0);

    _cairo_gl_context_release (ctx);
    if (vertices != vertices_stack)
	free (vertices);

    return CAIRO_STATUS_SUCCESS;
#undef N_STACK_RECTS
}


static cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void			   *abstract_surface,
				   cairo_operator_t	    op,
				   const cairo_color_t     *color,
				   cairo_rectangle_int_t   *rects,
				   int			    num_rects)
{
    cairo_gl_surface_t *surface = abstract_surface;

    if (_cairo_gl_device_has_glsl (surface->base.device)) {
	return _cairo_gl_surface_fill_rectangles_glsl(abstract_surface,
						      op,
						      color,
						      rects,
						      num_rects);
    } else {
	return _cairo_gl_surface_fill_rectangles_fixed(abstract_surface,
						       op,
						       color,
						       rects,
						       num_rects);
    }
}

typedef struct _cairo_gl_surface_span_renderer {
    cairo_span_renderer_t base;

    cairo_gl_composite_t setup;

    int xmin, xmax;

    cairo_gl_context_t *ctx;
} cairo_gl_surface_span_renderer_t;

static cairo_status_t
_cairo_gl_render_bounded_spans (void *abstract_renderer,
				int y, int height,
				const cairo_half_open_span_t *spans,
				unsigned num_spans)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (spans[0].coverage) {
            _cairo_gl_composite_emit_rect (renderer->ctx,
                                           &renderer->setup,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           spans[0].coverage << 24);
	}

	spans++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_render_unbounded_spans (void *abstract_renderer,
				  int y, int height,
				  const cairo_half_open_span_t *spans,
				  unsigned num_spans)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (num_spans == 0) {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       &renderer->setup,
                                       renderer->xmin, y,
                                       renderer->xmax, y + height,
                                       0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (spans[0].x != renderer->xmin) {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       &renderer->setup,
                                       renderer->xmin, y,
                                       spans[0].x,     y + height,
                                       0);
    }

    do {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       &renderer->setup,
                                       spans[0].x, y,
                                       spans[1].x, y + height,
                                       spans[0].coverage << 24);
	spans++;
    } while (--num_spans > 1);

    if (spans[0].x != renderer->xmax) {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       &renderer->setup,
                                       spans[0].x,     y,
                                       renderer->xmax, y + height,
                                       0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gl_surface_span_renderer_destroy (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (!renderer)
	return;

    _cairo_gl_composite_fini (renderer->ctx, &renderer->setup);

    _cairo_gl_context_release (renderer->ctx);

    free (renderer);
}

static cairo_status_t
_cairo_gl_surface_span_renderer_finish (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    _cairo_gl_composite_end (renderer->ctx, &renderer->setup);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_gl_surface_check_span_renderer (cairo_operator_t	  op,
				       const cairo_pattern_t  *pattern,
				       void			 *abstract_dst,
				       cairo_antialias_t	  antialias)
{
    cairo_surface_t *surface = abstract_dst;

    if (! _cairo_gl_operator_is_supported (op))
	return FALSE;

    if (! cairo_gl_device_check_span_renderer (surface->device))
	return FALSE;

    return TRUE;

    (void) pattern;
    (void) antialias;
}

static cairo_span_renderer_t *
_cairo_gl_surface_create_span_renderer (cairo_operator_t	 op,
					const cairo_pattern_t	*src,
					void			*abstract_dst,
					cairo_antialias_t	 antialias,
					const cairo_composite_rectangles_t *rects,
					cairo_region_t		*clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_surface_span_renderer_t *renderer;
    cairo_status_t status;
    const cairo_rectangle_int_t *extents;

    renderer = calloc (1, sizeof (*renderer));
    if (unlikely (renderer == NULL))
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_gl_surface_span_renderer_destroy;
    renderer->base.finish = _cairo_gl_surface_span_renderer_finish;
    if (rects->is_bounded) {
	renderer->base.render_rows = _cairo_gl_render_bounded_spans;
	extents = &rects->bounded;
    } else {
	renderer->base.render_rows = _cairo_gl_render_unbounded_spans;
	extents = &rects->unbounded;
    }
    renderer->xmin = extents->x;
    renderer->xmax = extents->x + extents->width;

    status = _cairo_gl_context_acquire (dst->base.device, &renderer->ctx);
    if (unlikely (status)) {
	free (renderer);
	return _cairo_span_renderer_create_in_error (status);
    }

    status = _cairo_gl_composite_init (renderer->ctx,
                                       &renderer->setup,
                                       op, dst, src, NULL,
                                       FALSE, extents);
    if (unlikely (status))
        goto FAIL;

    status = _cairo_gl_operand_init (renderer->ctx,
                                     &renderer->setup.src, src, dst,
				     rects->source.x, rects->source.y,
				     extents->x, extents->y,
				     extents->width, extents->height);
    if (unlikely (status))
        goto FAIL;

    _cairo_gl_composite_set_mask_spans (renderer->ctx, &renderer->setup);
    _cairo_gl_composite_set_clip_region (renderer->ctx, &renderer->setup, clip_region);

    status = _cairo_gl_composite_begin (renderer->ctx, &renderer->setup);
    if (unlikely (status))
        goto FAIL;

    return &renderer->base;


FAIL:
    _cairo_gl_composite_fini (renderer->ctx, &renderer->setup);
    _cairo_gl_context_release (renderer->ctx);
    free (renderer);
    return _cairo_span_renderer_create_in_error (status);
}

static cairo_bool_t
_cairo_gl_surface_get_extents (void		     *abstract_surface,
			       cairo_rectangle_int_t *rectangle)
{
    cairo_gl_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return TRUE;
}

static void
_cairo_gl_surface_get_font_options (void                  *abstract_surface,
				    cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
}


static cairo_int_status_t
_cairo_gl_surface_paint (void *abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t *source,
			 cairo_clip_t	    *clip)
{
    /* simplify the common case of clearing the surface */
    if (op == CAIRO_OPERATOR_CLEAR && clip == NULL)
	return _cairo_gl_surface_clear (abstract_surface);

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

const cairo_surface_backend_t _cairo_gl_surface_backend = {
    CAIRO_SURFACE_TYPE_GL,
    _cairo_gl_surface_create_similar,
    _cairo_gl_surface_finish,

    _cairo_gl_surface_acquire_source_image,
    _cairo_gl_surface_release_source_image,
    _cairo_gl_surface_acquire_dest_image,
    _cairo_gl_surface_release_dest_image,

    _cairo_gl_surface_clone_similar,
    _cairo_gl_surface_composite,
    _cairo_gl_surface_fill_rectangles,
    _cairo_gl_surface_composite_trapezoids,
    _cairo_gl_surface_create_span_renderer,
    _cairo_gl_surface_check_span_renderer,

    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_gl_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_gl_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    _cairo_gl_surface_scaled_font_fini,
    _cairo_gl_surface_scaled_glyph_fini,
    _cairo_gl_surface_paint,
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    _cairo_gl_surface_show_glyphs, /* show_glyphs */
    NULL  /* snapshot */
};
