/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006, 2008 Red Hat, Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 *      Kristian Høgsberg <krh@redhat.com>
 *      Behdad Esfahbod <behdad@behdad.org>
 */

#include "cairoint.h"
#include "cairo-meta-surface-private.h"
#include "cairo-analysis-surface-private.h"

typedef struct _cairo_user_scaled_font_methods {
    cairo_user_scaled_font_init_func_t			init;
    cairo_user_scaled_font_render_glyph_func_t		render_glyph;
    cairo_user_scaled_font_unicode_to_glyph_func_t	unicode_to_glyph;
    cairo_user_scaled_font_text_to_glyphs_func_t	text_to_glyphs;
} cairo_user_scaled_font_methods_t;

typedef struct _cairo_user_font_face {
    cairo_font_face_t	             base;

    /* Set to true after first scaled font is created.  At that point,
     * the scaled_font_methods cannot change anymore. */
    cairo_bool_t		     immutable;

    cairo_user_scaled_font_methods_t scaled_font_methods;
} cairo_user_font_face_t;

typedef struct _cairo_user_scaled_font {
    cairo_scaled_font_t  base;

    cairo_text_extents_t default_glyph_extents;
} cairo_user_scaled_font_t;

/* TODO test user fonts using other fonts in the render_glyph */

/* #cairo_user_scaled_font_t */

static const cairo_scaled_font_backend_t cairo_user_scaled_font_backend;

static cairo_int_status_t
_cairo_user_scaled_glyph_init (void			 *abstract_font,
			       cairo_scaled_glyph_t	 *scaled_glyph,
			       cairo_scaled_glyph_info_t  info)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_user_scaled_font_t *scaled_font = abstract_font;
    cairo_surface_t *meta_surface = scaled_glyph->meta_surface;

    if (!scaled_glyph->meta_surface) {
	cairo_user_font_face_t *face =
	    (cairo_user_font_face_t *) scaled_font->base.font_face;
	cairo_text_extents_t extents = scaled_font->default_glyph_extents;
	cairo_content_t content = scaled_font->base.options.antialias == CAIRO_ANTIALIAS_SUBPIXEL ?
									 CAIRO_CONTENT_COLOR_ALPHA :
									 CAIRO_CONTENT_ALPHA;
	cairo_t *cr;

	meta_surface = _cairo_meta_surface_create (content, -1, -1);
	cr = cairo_create (meta_surface);

	cairo_set_matrix (cr, &scaled_font->base.scale);
	cairo_set_font_size (cr, 1.0);
	cairo_set_font_options (cr, &scaled_font->base.options);

	status = face->scaled_font_methods.render_glyph ((cairo_scaled_font_t *)scaled_font,
							 _cairo_scaled_glyph_index(scaled_glyph),
							 cr, &extents);

	if (status == CAIRO_STATUS_SUCCESS)
	    status = cairo_status (cr);

	cairo_destroy (cr);

	if (status) {
	    cairo_surface_destroy (meta_surface);
	    return status;
	}

	_cairo_scaled_glyph_set_meta_surface (scaled_glyph,
					      &scaled_font->base,
					      meta_surface);


	/* set metrics */

	if (extents.width == 0.) {
	    /* Compute extents.x/y/width/height from meta_surface, in font space */

	    cairo_matrix_t matrix;
	    double fixed_scale, x_scale, y_scale;

	    cairo_box_t bbox;
	    double x1, y1, x2, y2;
	    cairo_surface_t *null_surface = _cairo_null_surface_create (cairo_surface_get_content (meta_surface));
	    cairo_surface_t *analysis_surface = _cairo_analysis_surface_create (null_surface, -1, -1);
	    cairo_surface_destroy (null_surface);

	    /* compute a normalized version of font scale matrix to compute
	     * extents in.  This is to minimize error caused by the cairo_fixed_t
	     * representation. */

	    matrix = scaled_font->base.scale_inverse;
	    status = _cairo_matrix_compute_scale_factors (&matrix, &x_scale, &y_scale, 1);
	    if (status)
		return status;

	    /* since glyphs are pretty much 1.0x1.0, we can reduce error by
	     * scaling to a larger square.  say, 1024.x1024. */
	    fixed_scale = 1024.;
	    x_scale /= fixed_scale;
	    y_scale /= fixed_scale;
	    if (x_scale == 0) x_scale = 1. / fixed_scale;
	    if (y_scale == 0) y_scale = 1. / fixed_scale;

	    cairo_matrix_scale (&matrix, 1. / x_scale, 1. / y_scale);

	    _cairo_analysis_surface_set_ctm (analysis_surface, &matrix);
	    status = _cairo_meta_surface_replay (meta_surface, analysis_surface);
	    _cairo_analysis_surface_get_bounding_box (analysis_surface, &bbox);
	    cairo_surface_destroy (analysis_surface);

	    _cairo_box_to_doubles (&bbox, &x1, &y1, &x2, &y2);

	    extents.x_bearing = x1 * x_scale;
	    extents.y_bearing = y1 * y_scale;
	    extents.width     = (x2 - x1) * x_scale;
	    extents.height    = (y2 - y1) * y_scale;
	}

	_cairo_scaled_glyph_set_metrics (scaled_glyph,
					 &scaled_font->base,
					 &extents);
    }

    if (info & CAIRO_SCALED_GLYPH_INFO_SURFACE) {
	cairo_surface_t	*surface;
	cairo_status_t status = CAIRO_STATUS_SUCCESS;
	cairo_format_t format;
	int width, height;

	/* TODO
	 * extend the glyph cache to support argb glyphs.
	 * need to figure out the semantics and interaction with subpixel
	 * rendering first.
	 */

	width = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.x) -
	  _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x);
	height = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.y) -
	  _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y);

	switch (scaled_font->base.options.antialias) {
	default:
	case CAIRO_ANTIALIAS_DEFAULT:
	case CAIRO_ANTIALIAS_GRAY:	format = CAIRO_FORMAT_A8;	break;
	case CAIRO_ANTIALIAS_NONE:	format = CAIRO_FORMAT_A1;	break;
	case CAIRO_ANTIALIAS_SUBPIXEL:	format = CAIRO_FORMAT_ARGB32;	break;
	}
	surface = cairo_image_surface_create (format, width, height);

	cairo_surface_set_device_offset (surface,
	                                 - _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x),
	                                 - _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y));
	status = _cairo_meta_surface_replay (meta_surface, surface);

	if (status) {
	    cairo_surface_destroy(surface);
	    return status;
	}

	_cairo_scaled_glyph_set_surface (scaled_glyph,
					 &scaled_font->base,
					 (cairo_image_surface_t *) surface);
    }

    if (info & CAIRO_SCALED_GLYPH_INFO_PATH) {
	cairo_path_fixed_t *path = _cairo_path_fixed_create ();
	if (!path)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_meta_surface_get_path (meta_surface, path);

	if (status) {
	    _cairo_path_fixed_destroy (path);
	    return status;
	}

	_cairo_scaled_glyph_set_path (scaled_glyph,
				      &scaled_font->base,
				      path);
    }

    return status;
}

static unsigned long
_cairo_user_ucs4_to_index (void	    *abstract_font,
			   uint32_t  ucs4)
{
    cairo_user_scaled_font_t *scaled_font = abstract_font;
    cairo_user_font_face_t *face =
	(cairo_user_font_face_t *) scaled_font->base.font_face;
    unsigned long glyph = 0;

    if (face->scaled_font_methods.unicode_to_glyph) {
	cairo_status_t status;

	status = face->scaled_font_methods.unicode_to_glyph (&scaled_font->base,
							     ucs4, &glyph);

	if (status != CAIRO_STATUS_SUCCESS) {
	    status = _cairo_scaled_font_set_error (&scaled_font->base, status);
	    glyph = 0;
	}

    } else {
	glyph = ucs4;
    }

    return glyph;
}

static cairo_int_status_t
_cairo_user_text_to_glyphs (void           *abstract_font,
			    double          x,
			    double          y,
			    const char     *utf8,
			    cairo_glyph_t **glyphs,
			    int	           *num_glyphs)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_user_scaled_font_t *scaled_font = abstract_font;
    cairo_user_font_face_t *face =
	(cairo_user_font_face_t *) scaled_font->base.font_face;

    if (face->scaled_font_methods.text_to_glyphs) {
	int i;

	*glyphs = NULL;

	status = face->scaled_font_methods.text_to_glyphs (&scaled_font->base,
							   utf8, glyphs, num_glyphs);

	if (status != CAIRO_STATUS_SUCCESS) {
	    status = _cairo_scaled_font_set_error (&scaled_font->base, status);
	    if (*glyphs) {
		free (*glyphs);
		*glyphs = NULL;
	    }
	    return status;
	}

	/* Convert from font space to user space and add x,y */
	for (i = 0; i < *num_glyphs; i++) {
	    double gx = (*glyphs)[i].x;
	    double gy = (*glyphs)[i].y;

	    cairo_matrix_transform_point (&scaled_font->base.font_matrix,
					  &gx, &gy);

	    (*glyphs)[i].x = gx + x;
	    (*glyphs)[i].y = gy + y;
	}
    }

    return status;
}

static const cairo_scaled_font_backend_t cairo_user_scaled_font_backend = {
    CAIRO_FONT_TYPE_USER,
    NULL,	/* create_toy */
    NULL,	/* scaled_font_fini */
    _cairo_user_scaled_glyph_init,
    _cairo_user_text_to_glyphs,
    _cairo_user_ucs4_to_index,
    NULL,	/* show_glyphs */
    NULL,	/* load_truetype_table */
    NULL,	/* map_glyphs_to_unicode */
};

/* #cairo_user_font_face_t */

static cairo_status_t
_cairo_user_font_face_scaled_font_create (void                        *abstract_face,
					  const cairo_matrix_t        *font_matrix,
					  const cairo_matrix_t        *ctm,
					  const cairo_font_options_t  *options,
					  cairo_scaled_font_t        **scaled_font)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_user_font_face_t *font_face = abstract_face;
    cairo_user_scaled_font_t *user_scaled_font = NULL;
    cairo_font_extents_t font_extents = {1., 0., 1., 1., 0.};

    font_face->immutable = TRUE;

    user_scaled_font = malloc (sizeof (cairo_user_scaled_font_t));
    if (user_scaled_font == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    status = _cairo_scaled_font_init (&user_scaled_font->base,
				      &font_face->base,
				      font_matrix, ctm, options,
				      &cairo_user_scaled_font_backend);

    if (status) {
	free (user_scaled_font);
	return status;
    }

    if (font_face->scaled_font_methods.init != NULL) {

	/* Lock the scaled_font mutex such that user doesn't accidentally try
         * to use it just yet. */
	CAIRO_MUTEX_LOCK (user_scaled_font->base.mutex);

	/* Give away fontmap lock such that user-font can use other fonts */
	_cairo_scaled_font_register_placeholder_and_unlock_font_map (&user_scaled_font->base);

	status = font_face->scaled_font_methods.init (&user_scaled_font->base,
						      &font_extents);

	_cairo_scaled_font_unregister_placeholder_and_lock_font_map (&user_scaled_font->base);

	CAIRO_MUTEX_UNLOCK (user_scaled_font->base.mutex);
    }

    if (status == CAIRO_STATUS_SUCCESS)
	status = _cairo_scaled_font_set_metrics (&user_scaled_font->base, &font_extents);


    if (status != CAIRO_STATUS_SUCCESS) {
        _cairo_scaled_font_fini (&user_scaled_font->base);
	free (user_scaled_font);
    } else {
        user_scaled_font->default_glyph_extents.x_bearing = 0.;
        user_scaled_font->default_glyph_extents.y_bearing = -font_extents.ascent;
        user_scaled_font->default_glyph_extents.width = 0.;
        user_scaled_font->default_glyph_extents.height = font_extents.ascent + font_extents.descent;
        user_scaled_font->default_glyph_extents.x_advance = font_extents.max_x_advance;
        user_scaled_font->default_glyph_extents.y_advance = 0.;

	*scaled_font = &user_scaled_font->base;
    }

    return status;
}

static const cairo_font_face_backend_t _cairo_user_font_face_backend = {
    CAIRO_FONT_TYPE_USER,
    NULL,	/* destroy */
    _cairo_user_font_face_scaled_font_create
};


/* Implement the public interface */

/**
 * cairo_user_font_face_create:
 *
 * TODO: document this
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 *
 * Since: 1.8
 **/
cairo_font_face_t *
cairo_user_font_face_create (void)
{
    cairo_user_font_face_t *font_face;

    font_face = malloc (sizeof (cairo_user_font_face_t));
    if (!font_face) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_font_face_t *)&_cairo_font_face_nil;
    }

    _cairo_font_face_init (&font_face->base, &_cairo_user_font_face_backend);

    font_face->immutable = FALSE;
    memset (&font_face->scaled_font_methods, 0, sizeof (font_face->scaled_font_methods));

    return &font_face->base;
}

/* User-font method setters */

static cairo_bool_t
_cairo_font_face_is_user (cairo_font_face_t *font_face)
{
    return font_face->backend == &_cairo_user_font_face_backend;
}


void
cairo_user_font_face_set_init_func (cairo_font_face_t                  *font_face,
				    cairo_user_scaled_font_init_func_t  init_func)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.init = init_func;
}

void
cairo_user_font_face_set_render_glyph_func (cairo_font_face_t                          *font_face,
					    cairo_user_scaled_font_render_glyph_func_t  render_glyph_func)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.render_glyph = render_glyph_func;
}

void
cairo_user_font_face_set_unicode_to_glyph_func (cairo_font_face_t                              *font_face,
						cairo_user_scaled_font_unicode_to_glyph_func_t  unicode_to_glyph_func)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.unicode_to_glyph = unicode_to_glyph_func;
}

void
cairo_user_font_face_set_text_to_glyphs_func (cairo_font_face_t                            *font_face,
					      cairo_user_scaled_font_text_to_glyphs_func_t  text_to_glyphs_func)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.text_to_glyphs = text_to_glyphs_func;
}

/* User-font method getters */

cairo_user_scaled_font_init_func_t
cairo_user_font_face_get_init_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }
    return user_font_face->scaled_font_methods.init;
}

cairo_user_scaled_font_render_glyph_func_t
cairo_user_font_face_get_render_glyph_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }
    return user_font_face->scaled_font_methods.render_glyph;
}

cairo_user_scaled_font_unicode_to_glyph_func_t
cairo_user_font_face_get_unicode_to_glyph_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }
    return user_font_face->scaled_font_methods.unicode_to_glyph;
}

cairo_user_scaled_font_text_to_glyphs_func_t
cairo_user_font_face_get_text_to_glyphs_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face = (cairo_user_font_face_t *) font_face;
    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }
    return user_font_face->scaled_font_methods.text_to_glyphs;
}
