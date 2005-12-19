/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include <stdlib.h>

#include "cairoint.h"

#include "cairo-clip-private.h"
#include "cairo-gstate-private.h"

static cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target);

static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other);

static void
_cairo_gstate_fini (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate);

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate);

/**
 * _cairo_gstate_create:
 * @target: a #cairo_surface_t, not NULL
 *
 * Create a new #cairo_gstate_t to draw to target with all graphics
 * state parameters set to defaults. gstate->next will be set to NULL
 * and may be used by the caller to chain #cairo_gstate_t objects
 * together.
 *
 * Return value: a new #cairo_gstate_t or NULL if there is
 * insufficient memory.
 **/
cairo_gstate_t *
_cairo_gstate_create (cairo_surface_t *target)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    assert (target != NULL);

    gstate = malloc (sizeof (cairo_gstate_t));
    if (gstate == NULL)
	return NULL;

    status = _cairo_gstate_init (gstate, target);
    if (status) {
	free (gstate);
	return NULL;		
    }

    return gstate;
}

static cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target)
{
    gstate->op = CAIRO_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;
    gstate->antialias = CAIRO_ANTIALIAS_DEFAULT;

    _cairo_stroke_style_init (&gstate->stroke_style);

    gstate->fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;

    gstate->font_face = NULL;
    gstate->scaled_font = NULL;

    cairo_matrix_init_scale (&gstate->font_matrix,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE, 
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE);

    _cairo_font_options_init_default (&gstate->font_options);
    
    _cairo_clip_init (&gstate->clip, target);

    gstate->target = cairo_surface_reference (target);

    _cairo_gstate_identity_matrix (gstate);
    gstate->source_ctm_inverse = gstate->ctm_inverse;

    gstate->source = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK);
    if (gstate->source->status)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_init_copy:
 *
 * Initialize @gstate by performing a deep copy of state fields from
 * @other. Note that gstate->next is not copied but is set to NULL by
 * this function.
 **/
static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other)
{
    cairo_status_t status;
    
    gstate->op = other->op;

    gstate->tolerance = other->tolerance;
    gstate->antialias = other->antialias;

    status = _cairo_stroke_style_init_copy (&gstate->stroke_style,
					    &other->stroke_style);
    if (status)
	return status;

    gstate->fill_rule = other->fill_rule;

    gstate->font_face = cairo_font_face_reference (other->font_face);
    gstate->scaled_font = cairo_scaled_font_reference (other->scaled_font);

    gstate->font_matrix = other->font_matrix;

    _cairo_font_options_init_copy (&gstate->font_options , &other->font_options);

    _cairo_clip_init_copy (&gstate->clip, &other->clip);

    gstate->target = cairo_surface_reference (other->target);

    gstate->ctm = other->ctm;
    gstate->ctm_inverse = other->ctm_inverse;
    gstate->source_ctm_inverse = other->source_ctm_inverse;

    gstate->source = cairo_pattern_reference (other->source);

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    _cairo_stroke_style_fini (&gstate->stroke_style);

    cairo_font_face_destroy (gstate->font_face);
    gstate->font_face = NULL;

    cairo_scaled_font_destroy (gstate->scaled_font);
    gstate->scaled_font = NULL;

    _cairo_clip_fini (&gstate->clip);

    cairo_surface_destroy (gstate->target);
    gstate->target = NULL;

    cairo_pattern_destroy (gstate->source);
    gstate->source = NULL;
}

void
_cairo_gstate_destroy (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return;

    _cairo_gstate_fini (gstate);
    free (gstate);
}

/**
 * _cairo_gstate_clone:
 * @other: a #cairo_gstate_t to be copied, not NULL.
 *
 * Create a new #cairo_gstate_t setting all graphics state parameters
 * to the same values as contained in @other. gstate->next will be set
 * to NULL and may be used by the caller to chain cairo_gstate_t
 * objects together.
 *
 * Return value: a new cairo_gstate_t or NULL if there is insufficient
 * memory.
 **/
cairo_gstate_t*
_cairo_gstate_clone (cairo_gstate_t *other)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    assert (other != NULL);

    gstate = malloc (sizeof (cairo_gstate_t));
    if (gstate == NULL)
	return NULL;

    status = _cairo_gstate_init_copy (gstate, other);
    if (status) {
	free (gstate);
	return NULL;
    }

    return gstate;
}

/* Push rendering off to an off-screen group. */
/* XXX: Rethinking this API
cairo_status_t
_cairo_gstate_begin_group (cairo_gstate_t *gstate)
{
    Pixmap pix;
    unsigned int width, height;

    gstate->parent_surface = gstate->target;

    width = _cairo_surface_get_width (gstate->target);
    height = _cairo_surface_get_height (gstate->target);

    pix = XCreatePixmap (gstate->dpy,
			 _cairo_surface_get_drawable (gstate->target),
			 width, height,
			 _cairo_surface_get_depth (gstate->target));
    if (pix == 0)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->target = cairo_surface_create (gstate->dpy);
    if (gstate->target->status)
	return gstate->target->status;

    _cairo_surface_set_drawableWH (gstate->target, pix, width, height);

    status = _cairo_surface_fill_rectangle (gstate->target,
                                   CAIRO_OPERATOR_CLEAR,
				   CAIRO_COLOR_TRANSPARENT,
				   0, 0,
			           _cairo_surface_get_width (gstate->target),
				   _cairo_surface_get_height (gstate->target));
    if (status)				 
        return status;

    return CAIRO_STATUS_SUCCESS;
}
*/

/* Complete the current offscreen group, composing its contents onto the parent surface. */
/* XXX: Rethinking this API
cairo_status_t
_cairo_gstate_end_group (cairo_gstate_t *gstate)
{
    Pixmap pix;
    cairo_color_t mask_color;
    cairo_surface_t mask;

    if (gstate->parent_surface == NULL)
	return CAIRO_STATUS_INVALID_POP_GROUP;

    _cairo_surface_init (&mask, gstate->dpy);
    _cairo_color_init (&mask_color);

    _cairo_surface_set_solid_color (&mask, &mask_color);

    * XXX: This could be made much more efficient by using
       _cairo_surface_get_damaged_width/Height if cairo_surface_t actually kept
       track of such informaton. *
    _cairo_surface_composite (gstate->op,
			      gstate->target,
			      mask,
			      gstate->parent_surface,
			      0, 0,
			      0, 0,
			      0, 0,
			      _cairo_surface_get_width (gstate->target),
			      _cairo_surface_get_height (gstate->target));

    _cairo_surface_fini (&mask);

    pix = _cairo_surface_get_drawable (gstate->target);
    XFreePixmap (gstate->dpy, pix);

    cairo_surface_destroy (gstate->target);
    gstate->target = gstate->parent_surface;
    gstate->parent_surface = NULL;

    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_surface_t *
_cairo_gstate_get_target (cairo_gstate_t *gstate)
{
    return gstate->target;
}

cairo_status_t
_cairo_gstate_set_source (cairo_gstate_t  *gstate,
			  cairo_pattern_t *source)
{
    if (source->status)
	return source->status;

    cairo_pattern_reference (source);
    cairo_pattern_destroy (gstate->source);
    gstate->source = source;
    gstate->source_ctm_inverse = gstate->ctm_inverse;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_pattern_t *
_cairo_gstate_get_source (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->source;
}

cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t op)
{
    gstate->op = op;

    return CAIRO_STATUS_SUCCESS;
}

cairo_operator_t
_cairo_gstate_get_operator (cairo_gstate_t *gstate)
{
    return gstate->op;
}

cairo_status_t
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance)
{
    gstate->tolerance = tolerance;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_tolerance (cairo_gstate_t *gstate)
{
    return gstate->tolerance;
}

cairo_status_t
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule)
{
    gstate->fill_rule = fill_rule;

    return CAIRO_STATUS_SUCCESS;
}

cairo_fill_rule_t
_cairo_gstate_get_fill_rule (cairo_gstate_t *gstate)
{
    return gstate->fill_rule;
}

cairo_status_t
_cairo_gstate_set_line_width (cairo_gstate_t *gstate, double width)
{
    gstate->stroke_style.line_width = width;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_line_width (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_width;
}

cairo_status_t
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap)
{
    gstate->stroke_style.line_cap = line_cap;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_cap_t
_cairo_gstate_get_line_cap (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_cap;
}

cairo_status_t
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join)
{
    gstate->stroke_style.line_join = line_join;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_join_t
_cairo_gstate_get_line_join (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_join;
}

cairo_status_t
_cairo_gstate_set_dash (cairo_gstate_t *gstate, double *dash, int num_dashes, double offset)
{
    int i;
    double dash_total;

    if (gstate->stroke_style.dash)
	free (gstate->stroke_style.dash);
    
    gstate->stroke_style.num_dashes = num_dashes;

    if (gstate->stroke_style.num_dashes == 0) {
	gstate->stroke_style.dash = NULL;
	gstate->stroke_style.dash_offset = 0.0;
	return CAIRO_STATUS_SUCCESS;
    }

    gstate->stroke_style.dash = malloc (gstate->stroke_style.num_dashes * sizeof (double));
    if (gstate->stroke_style.dash == NULL) {
	gstate->stroke_style.num_dashes = 0;
	return CAIRO_STATUS_NO_MEMORY;
    }

    memcpy (gstate->stroke_style.dash, dash, gstate->stroke_style.num_dashes * sizeof (double));
    
    dash_total = 0.0;
    for (i = 0; i < gstate->stroke_style.num_dashes; i++) {
	if (gstate->stroke_style.dash[i] < 0)
	    return CAIRO_STATUS_INVALID_DASH;
	dash_total += gstate->stroke_style.dash[i];
    }

    if (dash_total == 0.0)
	return CAIRO_STATUS_INVALID_DASH;

    /* A single dash value indicate symmetric repeating, so the total
     * is twice as long. */
    if (gstate->stroke_style.num_dashes == 1)
	dash_total *= 2;

    /* The dashing code doesn't like a negative offset, so we compute
     * the equivalent positive offset. */
    if (offset < 0)
	offset += ceil (-offset / dash_total + 0.5) * dash_total;

    gstate->stroke_style.dash_offset = offset;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit)
{
    gstate->stroke_style.miter_limit = limit;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_miter_limit (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.miter_limit;
}

static void
_cairo_gstate_apply_device_transform (cairo_gstate_t    *gstate,
				      cairo_matrix_t	*matrix)
{
    if (gstate->target->device_x_scale != 1.0 ||
	gstate->target->device_y_scale != 1.0)
    {
	cairo_matrix_scale (matrix,
			    gstate->target->device_x_scale,
			    gstate->target->device_y_scale);
    }
}

static void
_cairo_gstate_apply_device_inverse_transform (cairo_gstate_t    *gstate,
					      cairo_matrix_t	*matrix)
{
    if (gstate->target->device_x_scale != 1.0 ||
	gstate->target->device_y_scale != 1.0)
    {
	cairo_matrix_scale (matrix,
			    1/gstate->target->device_x_scale,
			    1/gstate->target->device_y_scale);
    }
}

void
_cairo_gstate_get_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix)
{
    *matrix = gstate->ctm;
    _cairo_gstate_apply_device_inverse_transform (gstate, matrix);
}

cairo_status_t
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_translate (&tmp, tx, ty);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_translate (&tmp, -tx, -ty);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy)
{
    cairo_matrix_t tmp;

    if (sx == 0 || sy == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_scale (&tmp, sx, sy);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_scale (&tmp, 1/sx, 1/sy);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_rotate (&tmp, angle);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_rotate (&tmp, -angle);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform (cairo_gstate_t	      *gstate,
			 const cairo_matrix_t *matrix)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    tmp = *matrix;
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_invert (&tmp);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_matrix (cairo_gstate_t       *gstate,
			  const cairo_matrix_t *matrix)
{
    cairo_status_t status;

    _cairo_gstate_unset_scaled_font (gstate);
    
    gstate->ctm = *matrix;

    gstate->ctm_inverse = *matrix;
    status = cairo_matrix_invert (&gstate->ctm_inverse);
    if (status)
	return status;

    _cairo_gstate_apply_device_transform (gstate, &gstate->ctm);
    _cairo_gstate_apply_device_inverse_transform (gstate, &gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate)
{
    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_identity (&gstate->ctm);
    cairo_matrix_init_identity (&gstate->ctm_inverse);

    _cairo_gstate_apply_device_transform (gstate, &gstate->ctm);
    _cairo_gstate_apply_device_inverse_transform (gstate, &gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm_inverse, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_user_to_backend (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);
    *x += gstate->target->device_x_offset;
    *y += gstate->target->device_y_offset;
}

void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    *x -= gstate->target->device_x_offset;
    *y -= gstate->target->device_y_offset;
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);
}

/* XXX: NYI 
cairo_status_t
_cairo_gstate_stroke_to_path (cairo_gstate_t *gstate)
{
    cairo_status_t status;

    _cairo_pen_init (&gstate);
    return CAIRO_STATUS_SUCCESS;
}
*/

static void
_cairo_gstate_copy_transformed_pattern (cairo_gstate_t  *gstate,
					cairo_pattern_t *pattern,
					cairo_pattern_t *original,
					cairo_matrix_t  *ctm_inverse)
{
    cairo_matrix_t tmp_matrix = *ctm_inverse;
  
    _cairo_pattern_init_copy (pattern, original);

    if (gstate->target)
	cairo_matrix_translate (&tmp_matrix,
				- gstate->target->device_x_offset,
				- gstate->target->device_y_offset);

    _cairo_pattern_transform (pattern, &tmp_matrix);
}

static void
_cairo_gstate_copy_transformed_source (cairo_gstate_t  *gstate,
				       cairo_pattern_t *pattern)
{
    _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					    gstate->source,
					    &gstate->source_ctm_inverse);
}

static void
_cairo_gstate_copy_transformed_mask (cairo_gstate_t  *gstate,
				     cairo_pattern_t *pattern,
				     cairo_pattern_t *mask)
{
    _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					    mask,
					    &gstate->ctm_inverse);
}

cairo_status_t
_cairo_gstate_paint (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &pattern.base);

    status = _cairo_surface_paint (gstate->target,
				   gstate->op,
				   &pattern.base);

    _cairo_pattern_fini (&pattern.base);

    return status;
}

/**
 * _cairo_operator_bounded_by_mask:
 * @op: a #cairo_operator_t
 * 
 * A bounded operator is one where mask pixel
 * of zero results in no effect on the destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, draw trapezoids with an unbounded operator, the effect
 * extends past the bounding box of the trapezoids.
 *
 * Return value: %TRUE if the operator is bounded by the mask operand
 **/
cairo_bool_t
_cairo_operator_bounded_by_mask (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }
    
    ASSERT_NOT_REACHED;
    return FALSE;
}

/**
 * _cairo_operator_bounded_by_source:
 * @op: a #cairo_operator_t
 * 
 * A bounded operator is one where source pixels of zero
 * (in all four components, r, g, b and a) effect no change
 * in the resulting destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, copy a surface with the SOURCE operator, the effect
 * extends past the bounding box of the source surface.
 *
 * Return value: %TRUE if the operator is bounded by the source operand
 **/
cairo_bool_t
_cairo_operator_bounded_by_source (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }
    
    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_status_t
_create_composite_mask_pattern (cairo_surface_pattern_t *mask_pattern,
				cairo_clip_t            *clip,
				cairo_draw_func_t        draw_func,
				void                    *draw_closure,
				cairo_surface_t         *dst,
				const cairo_rectangle_t *extents)
{
    cairo_surface_t *mask;
    cairo_status_t status;
    
    mask = cairo_surface_create_similar (dst,
					 CAIRO_CONTENT_ALPHA,
					 extents->width,
					 extents->height);
    if (mask->status)
	return CAIRO_STATUS_NO_MEMORY;
    
    status = (*draw_func) (draw_closure, CAIRO_OPERATOR_ADD,
			   NULL, mask,
			   extents->x, extents->y,
			   extents);
    if (status)
	goto CLEANUP_SURFACE;

    if (clip && clip->surface)
	status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_IN,
						 mask,
						 extents->x, extents->y,
						 extents);
    if (status)
	goto CLEANUP_SURFACE;
    
    _cairo_pattern_init_for_surface (mask_pattern, mask);

 CLEANUP_SURFACE:
    cairo_surface_destroy (mask);

    return status;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_cairo_gstate_clip_and_composite_with_mask (cairo_clip_t            *clip,
					    cairo_operator_t         op,
					    cairo_pattern_t         *src,
					    cairo_draw_func_t        draw_func,
					    void                    *draw_closure,
					    cairo_surface_t         *dst,
					    const cairo_rectangle_t *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_status_t status;

    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (status)
	return status;
	
    status = _cairo_surface_composite (op,
				       src, &mask_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&mask_pattern.base);

    return status;
}

/* Handles compositing with a clip surface when we have to do the operation
 * in two pieces and combine them together.
 */
static cairo_status_t
_cairo_gstate_clip_and_composite_combine (cairo_clip_t            *clip,
					  cairo_operator_t         op,
					  cairo_pattern_t         *src,
					  cairo_draw_func_t        draw_func,
					  void                    *draw_closure,
					  cairo_surface_t         *dst,
					  const cairo_rectangle_t *extents)
{
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t dst_pattern;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_status_t status;

    /* We'd be better off here creating a surface identical in format
     * to dst, but we have no way of getting that information.
     * A CAIRO_CONTENT_CLONE or something might be useful.
     * cairo_surface_create_similar() also unnecessarily clears the surface.
     */
    intermediate = cairo_surface_create_similar (dst,
						 CAIRO_CONTENT_COLOR_ALPHA,
						 extents->width,
						 extents->height);
    if (intermediate->status)
	return CAIRO_STATUS_NO_MEMORY;

    /* Initialize the intermediate surface from the destination surface
     */
    _cairo_pattern_init_for_surface (&dst_pattern, dst);

    status = _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
				       &dst_pattern.base, NULL, intermediate,
				       extents->x,     extents->y,
				       0,              0,
				       0,              0,
				       extents->width, extents->height);

    _cairo_pattern_fini (&dst_pattern.base);

    if (status)
	goto CLEANUP_SURFACE;

    status = (*draw_func) (draw_closure, op,
			   src, intermediate,
			   extents->x, extents->y,
			   extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Combine that with the clip
     */
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_DEST_IN,
					     intermediate,
					     extents->x, extents->y,					     
					     extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Punch the clip out of the destination
     */
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_DEST_OUT,
					     dst,
					     0, 0,
					     extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Now add the two results together
     */
    _cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
    
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       &intermediate_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&intermediate_pattern.base);
    
 CLEANUP_SURFACE:
    cairo_surface_destroy (intermediate);

    return status;
}

/* Handles compositing for CAIRO_OPERATOR_SOURCE, which is special; it's
 * defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
 */
static cairo_status_t
_cairo_gstate_clip_and_composite_source (cairo_clip_t            *clip,
					 cairo_pattern_t         *src,
					 cairo_draw_func_t        draw_func,
					 void                    *draw_closure,
					 cairo_surface_t         *dst,
					 const cairo_rectangle_t *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_status_t status;

    /* Create a surface that is mask IN clip
     */
    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (status)
	return status;
    
    /* Compute dest' = dest OUT (mask IN clip)
     */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_OUT,
				       &mask_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    if (status)
	goto CLEANUP_MASK_PATTERN;

    /* Now compute (src IN (mask IN clip)) ADD dest'
     */
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       src, &mask_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

 CLEANUP_MASK_PATTERN:
    _cairo_pattern_fini (&mask_pattern.base);
    return status;
}

static int
_cairo_rectangle_empty (const cairo_rectangle_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/**
 * _cairo_gstate_clip_and_composite:
 * @gstate: a #cairo_gstate_t
 * @op: the operator to draw with
 * @src: source pattern
 * @draw_func: function that can be called to draw with the mask onto a surface.
 * @draw_closure: data to pass to @draw_func.
 * @dst: destination surface
 * @extents: rectangle holding a bounding box for the operation; this
 *           rectangle will be used as the size for the temporary
 *           surface.
 *
 * When there is a surface clip, we typically need to create an intermediate
 * surface. This function handles the logic of creating a temporary surface
 * drawing to it, then compositing the result onto the target surface.
 *
 * @draw_func is to called to draw the mask; it will be called no more
 * than once.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if the drawing succeeded.
 **/
cairo_status_t
_cairo_gstate_clip_and_composite (cairo_clip_t            *clip,
				  cairo_operator_t         op,
				  cairo_pattern_t         *src,
				  cairo_draw_func_t        draw_func,
				  void                    *draw_closure,
				  cairo_surface_t         *dst,
				  const cairo_rectangle_t *extents)
{
    cairo_pattern_union_t solid_pattern;
    cairo_status_t status;

    if (_cairo_rectangle_empty (extents))
	/* Nothing to do */
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_pattern_init_solid (&solid_pattern.solid, CAIRO_COLOR_WHITE);
	src = &solid_pattern.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if ((clip && clip->surface) || op == CAIRO_OPERATOR_SOURCE)
    {
	if (op == CAIRO_OPERATOR_SOURCE)
	    status = _cairo_gstate_clip_and_composite_source (clip,
							      src,
							      draw_func, draw_closure,
							      dst, extents);
	else if (_cairo_operator_bounded_by_mask (op))
	    status = _cairo_gstate_clip_and_composite_with_mask (clip, op,
								 src,
								 draw_func, draw_closure,
								 dst, extents);
	else
	    status = _cairo_gstate_clip_and_composite_combine (clip, op,
							       src,
							       draw_func, draw_closure,
							       dst, extents);
    }
    else
    {
	status = (*draw_func) (draw_closure, op,
			       src, dst,
			       0, 0,
			       extents);
    }

    if (src == &solid_pattern.base)
	_cairo_pattern_fini (&solid_pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_mask (cairo_gstate_t  *gstate,
		    cairo_pattern_t *mask)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern, mask_pattern;

    if (mask->status)
	return mask->status;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);
    _cairo_gstate_copy_transformed_mask (gstate, &mask_pattern.base, mask);

    status = _cairo_surface_mask (gstate->target,
				  gstate->op,
				  &source_pattern.base,
				  &mask_pattern.base);

    _cairo_pattern_fini (&source_pattern.base);
    _cairo_pattern_fini (&mask_pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern;

    if (gstate->source->status)
	return gstate->source->status;

    if (gstate->stroke_style.line_width <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);

    status = _cairo_surface_stroke (gstate->target,
				    gstate->op,
				    &source_pattern.base,
				    path,
				    &gstate->stroke_style,
				    &gstate->ctm,
				    &gstate->ctm_inverse,
				    gstate->tolerance,
				    gstate->antialias);

    _cairo_pattern_fini (&source_pattern.base);
    
    return status;

}

cairo_status_t
_cairo_gstate_in_stroke (cairo_gstate_t	    *gstate,
			 cairo_path_fixed_t *path,
			 double		     x,
			 double		     y,
			 cairo_bool_t	    *inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path,
						&gstate->stroke_style,
						&gstate->ctm,
						&gstate->ctm_inverse,
						gstate->tolerance,
						&traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

/* XXX We currently have a confusing mix of boxes and rectangles as
 * exemplified by this function.  A cairo_box_t is a rectangular area
 * represented by the coordinates of the upper left and lower right
 * corners, expressed in fixed point numbers.  A cairo_rectangle_t is
 * also a rectangular area, but represented by the upper left corner
 * and the width and the height, as integer numbers.
 *
 * This function converts a cairo_box_t to a cairo_rectangle_t by
 * increasing the area to the nearest integer coordinates.  We should
 * standardize on cairo_rectangle_t and cairo_rectangle_fixed_t, and
 * this function could be renamed to the more reasonable
 * _cairo_rectangle_fixed_round.
 */

void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

void
_cairo_rectangle_intersect (cairo_rectangle_t *dest, cairo_rectangle_t *src)
{
    int x1, y1, x2, y2;

    x1 = MAX (dest->x, src->x);
    y1 = MAX (dest->y, src->y);
    x2 = MIN (dest->x + dest->width, src->x + src->width);
    y2 = MIN (dest->y + dest->height, src->y + src->height);

    if (x1 >= x2 || y1 >= y2) {
	dest->x = 0;
	dest->y = 0;
	dest->width = 0;
	dest->height = 0;
    } else {
	dest->x = x1;
	dest->y = y1;
	dest->width = x2 - x1;
	dest->height = y2 - y1;
    }	
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_clip_t      *clip,
			cairo_pattern_t   *src,
			cairo_operator_t   op,
			cairo_surface_t   *dst,
			pixman_region16_t *trap_region,
			cairo_rectangle_t *extents)
{
    cairo_status_t status;
    cairo_pattern_union_t solid_pattern;
    cairo_pattern_union_t mask;
    int num_rects = pixman_region_num_rects (trap_region);
    unsigned int clip_serial;
    cairo_surface_t *clip_surface = clip ? clip->surface : NULL;

    if (clip_surface && op == CAIRO_OPERATOR_CLEAR) {
	_cairo_pattern_init_solid (&solid_pattern.solid, CAIRO_COLOR_WHITE);
	src = &solid_pattern.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;
    
    if (num_rects > 1) {
      if (_cairo_surface_get_clip_mode (dst) != CAIRO_CLIP_MODE_REGION)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	
	clip_serial = _cairo_surface_allocate_clip_serial (dst);
	status = _cairo_surface_set_clip_region (dst,
						 trap_region,
						 clip_serial);
	if (status)
	    return status;
    }
    
    if (clip_surface)
	_cairo_pattern_init_for_surface (&mask.surface, clip_surface);
	
    status = _cairo_surface_composite (op,
				       src,
				       clip_surface ? &mask.base : NULL,
				       dst,
				       extents->x, extents->y,
				       extents->x - (clip_surface ? clip->surface_rect.x : 0),
				       extents->y - (clip_surface ? clip->surface_rect.y : 0),
				       extents->x, extents->y,
				       extents->width, extents->height);

    if (clip_surface)
      _cairo_pattern_fini (&mask.base);

    if (src == &solid_pattern.base)
	_cairo_pattern_fini (&solid_pattern.base);

    return status;
}

typedef struct {
    cairo_traps_t *traps;
    cairo_antialias_t antialias;
} cairo_composite_traps_info_t;

static cairo_status_t
_composite_traps_draw_func (void                    *closure,
			    cairo_operator_t         op,
			    cairo_pattern_t         *src,
			    cairo_surface_t         *dst,
			    int                      dst_x,
			    int                      dst_y,
			    const cairo_rectangle_t *extents)
{
    cairo_composite_traps_info_t *info = closure;
    cairo_pattern_union_t pattern;
    cairo_status_t status;
    
    if (dst_x != 0 || dst_y != 0)
	_cairo_traps_translate (info->traps, - dst_x, - dst_y);

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    if (!src)
	src = &pattern.base;
    
    status = _cairo_surface_composite_trapezoids (op,
						  src, dst, info->antialias,
						  extents->x,         extents->y,
						  extents->x - dst_x, extents->y - dst_y,
						  extents->width,     extents->height,
						  info->traps->traps,
						  info->traps->num_traps);
    _cairo_pattern_fini (&pattern.base);

    return status;
}

/* Warning: This call modifies the coordinates of traps */
cairo_status_t
_cairo_surface_clip_and_composite_trapezoids (cairo_pattern_t *src,
					      cairo_operator_t op,
					      cairo_surface_t *dst,
					      cairo_traps_t *traps,
					      cairo_clip_t *clip,
					      cairo_antialias_t antialias)
{
    cairo_status_t status;
    pixman_region16_t *trap_region;
    pixman_region16_t *clear_region = NULL;
    cairo_rectangle_t extents;
    cairo_composite_traps_info_t traps_info;
    
    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_traps_extract_region (traps, &trap_region);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_mask (op))
    {
	if (trap_region) {
	    status = _cairo_clip_intersect_to_region (clip, trap_region);
	    _cairo_region_extents_rectangle (trap_region, &extents);
	} else {
	    cairo_box_t trap_extents;
	    _cairo_traps_extents (traps, &trap_extents);
	    _cairo_box_round_to_rectangle (&trap_extents, &extents);
	    status = _cairo_clip_intersect_to_rectangle (clip, &extents);
	}
    }
    else
    {
	cairo_surface_t *clip_surface = clip ? clip->surface : NULL;
	
	status = _cairo_surface_get_extents (dst, &extents);
	if (status)
	    return status;
	
	if (trap_region && !clip_surface) {
	    /* If we optimize drawing with an unbounded operator to
	     * _cairo_surface_fill_rectangles() or to drawing with a
	     * clip region, then we have an additional region to clear.
	     */
	    status = _cairo_surface_get_extents (dst, &extents);
	    if (status)
		return status;
	    
	    clear_region = _cairo_region_create_from_rectangle (&extents);
	    status = _cairo_clip_intersect_to_region (clip, clear_region);
	    if (status)
		return status;
	    
	    _cairo_region_extents_rectangle (clear_region,  &extents);
	    
	    if (pixman_region_subtract (clear_region, clear_region, trap_region) != PIXMAN_REGION_STATUS_SUCCESS)
		return CAIRO_STATUS_NO_MEMORY;
	    
	    if (!pixman_region_not_empty (clear_region)) {
		pixman_region_destroy (clear_region);
		clear_region = NULL;
	    }
	} else {
	    status = _cairo_clip_intersect_to_rectangle (clip, &extents);
	    if (status)
		return status;
	}
    }
	
    if (status)
	goto out;
    
    if (trap_region)
    {
	cairo_surface_t *clip_surface = clip ? clip->surface : NULL;
	
	if ((src->type == CAIRO_PATTERN_SOLID || op == CAIRO_OPERATOR_CLEAR) &&
	    !clip_surface)
	{
	    const cairo_color_t *color;

	    if (op == CAIRO_OPERATOR_CLEAR)
		color = CAIRO_COLOR_TRANSPARENT;
	    else
		color = &((cairo_solid_pattern_t *)src)->color;
	  
	    /* Solid rectangles special case */
	    status = _cairo_surface_fill_region (dst, op, color, trap_region);
	    if (!status && clear_region)
		status = _cairo_surface_fill_region (dst, CAIRO_OPERATOR_CLEAR,
						     CAIRO_COLOR_TRANSPARENT,
						     clear_region);

	    goto out;
	}

	if ((_cairo_operator_bounded_by_mask (op) && op != CAIRO_OPERATOR_SOURCE) ||
	    !clip_surface)
	{
	    /* For a simple rectangle, we can just use composite(), for more
	     * rectangles, we have to set a clip region. The cost of rasterizing
	     * trapezoids is pretty high for most backends currently, so it's
	     * worthwhile even if a region is needed.
	     *
	     * If we have a clip surface, we set it as the mask; this only works
	     * for bounded operators other than SOURCE; for unbounded operators,
	     * clip and mask cannot be interchanged. For SOURCE, the operator
	     * as implemented by the backends is different in it's handling
	     * of the mask then what we want.
	     *
	     * CAIRO_INT_STATUS_UNSUPPORTED will be returned if the region has
	     * more than rectangle and the destination doesn't support clip
	     * regions. In that case, we fall through.
	     */
	    status = _composite_trap_region (clip, src, op, dst,
					     trap_region, &extents);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    {
		if (!status && clear_region)
		    status = _cairo_surface_fill_region (dst, CAIRO_OPERATOR_CLEAR,
							 CAIRO_COLOR_TRANSPARENT,
							 clear_region);
		goto out;
	    }
	}
    }

    traps_info.traps = traps;
    traps_info.antialias = antialias;

    status = _cairo_gstate_clip_and_composite (clip, op, src,
					       _composite_traps_draw_func, &traps_info,
					       dst, &extents);

 out:
    if (trap_region)
	pixman_region_destroy (trap_region);
    if (clear_region)
	pixman_region_destroy (clear_region);
    
    return status;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;

    if (gstate->source->status)
	return gstate->source->status;
    
    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;
  
    _cairo_gstate_copy_transformed_source (gstate, &pattern.base);

    status = _cairo_surface_fill (gstate->target,
				  gstate->op,
				  &pattern.base,
				  path,
				  gstate->fill_rule,
				  gstate->tolerance,
				  gstate->antialias);

    _cairo_pattern_fini (&pattern.base);
    
    return status;
}

cairo_status_t
_cairo_gstate_in_fill (cairo_gstate_t	  *gstate,
		       cairo_path_fixed_t *path,
		       double		   x,
		       double		   y,
		       cairo_bool_t	  *inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);
    
BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_copy_page (cairo_gstate_t *gstate)
{
    return _cairo_surface_copy_page (gstate->target);
}

cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate)
{
    return _cairo_surface_show_page (gstate->target);
}

cairo_status_t
_cairo_gstate_stroke_extents (cairo_gstate_t	 *gstate,
			      cairo_path_fixed_t *path,
                              double *x1, double *y1,
			      double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_stroke_to_traps (path,
						&gstate->stroke_style,
						&gstate->ctm,
						&gstate->ctm_inverse,
						gstate->tolerance,
						&traps);
    if (status)
	goto BAIL;

    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_fill_extents (cairo_gstate_t     *gstate,
			    cairo_path_fixed_t *path,
                            double *x1, double *y1,
			    double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status)
	goto BAIL;
  
    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_reset_clip (cairo_gstate_t *gstate)
{
    return _cairo_clip_reset (&gstate->clip);
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    return _cairo_clip_clip (&gstate->clip,
			     path, gstate->fill_rule, gstate->tolerance,
			     gstate->antialias, gstate->target);
}

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate)
{
    if (gstate->scaled_font) {
	cairo_scaled_font_destroy (gstate->scaled_font);
	gstate->scaled_font = NULL;
    }
}

cairo_status_t
_cairo_gstate_select_font_face (cairo_gstate_t       *gstate, 
				const char           *family, 
				cairo_font_slant_t    slant, 
				cairo_font_weight_t   weight)
{
    cairo_font_face_t *font_face;

    font_face = _cairo_toy_font_face_create (family, slant, weight);
    if (font_face->status)
	return font_face->status;

    _cairo_gstate_set_font_face (gstate, font_face);
    cairo_font_face_destroy (font_face);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_size (cairo_gstate_t *gstate, 
			     double          size)
{
    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_scale (&gstate->font_matrix, size, size);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_matrix (cairo_gstate_t	    *gstate, 
			       const cairo_matrix_t *matrix)
{
    _cairo_gstate_unset_scaled_font (gstate);

    gstate->font_matrix = *matrix;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_get_font_matrix (cairo_gstate_t *gstate,
			       cairo_matrix_t *matrix)
{
    *matrix = gstate->font_matrix;
}

cairo_status_t
_cairo_gstate_set_font_options (cairo_gstate_t             *gstate,
				const cairo_font_options_t *options)
{
    _cairo_gstate_unset_scaled_font (gstate);

    gstate->font_options = *options;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_get_font_options (cairo_gstate_t       *gstate,
				cairo_font_options_t *options)
{
    *options = gstate->font_options;
}

cairo_status_t
_cairo_gstate_get_font_face (cairo_gstate_t     *gstate,
			     cairo_font_face_t **font_face)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;
    
    *font_face = gstate->font_face;

    return CAIRO_STATUS_SUCCESS;
}

/* 
 * Like everything else in this file, fonts involve Too Many Coordinate Spaces;
 * it is easy to get confused about what's going on.
 *
 * The user's view
 * ---------------
 *
 * Users ask for things in user space. When cairo starts, a user space unit
 * is about 1/96 inch, which is similar to (but importantly different from)
 * the normal "point" units most users think in terms of. When a user
 * selects a font, its scale is set to "one user unit". The user can then
 * independently scale the user coordinate system *or* the font matrix, in
 * order to adjust the rendered size of the font.
 *
 * Metrics are returned in user space, whether they are obtained from
 * the currently selected font in a  #cairo_t or from a #cairo_scaled_font_t
 * which is aa font specialized to a particular scale matrix, CTM, and target
 * surface. 
 *
 * The font's view
 * ---------------
 *
 * Fonts are designed and stored (in say .ttf files) in "font space", which
 * describes an "EM Square" (a design tile) and has some abstract number
 * such as 1000, 1024, or 2048 units per "EM". This is basically an
 * uninteresting space for us, but we need to remember that it exists.
 *
 * Font resources (from libraries or operating systems) render themselves
 * to a particular device. Since they do not want to make most programmers
 * worry about the font design space, the scaling API is simplified to
 * involve just telling the font the required pixel size of the EM square
 * (that is, in device space).
 *
 *
 * Cairo's gstate view
 * -------------------
 *
 * In addition to the CTM and CTM inverse, we keep a matrix in the gstate
 * called the "font matrix" which describes the user's most recent
 * font-scaling or font-transforming request. This is kept in terms of an
 * abstract scale factor, composed with the CTM and used to set the font's
 * pixel size. So if the user asks to "scale the font by 12", the matrix
 * is:
 *
 *   [ 12.0, 0.0, 0.0, 12.0, 0.0, 0.0 ]
 *
 * It is an affine matrix, like all cairo matrices, but its tx and ty
 * components are always set to zero; we don't permit "nudging" fonts
 * around.
 *
 * In order to perform any action on a font, we must build an object
 * called a cairo_font_scale_t; this contains the central 2x2 matrix 
 * resulting from "font matrix * CTM".
 *  
 * We pass this to the font when making requests of it, which causes it to
 * reply for a particular [user request, device] combination, under the CTM
 * (to accomodate the "zoom in" == "bigger fonts" issue above).
 *
 * The other terms in our communication with the font are therefore in
 * device space. When we ask it to perform text->glyph conversion, it will
 * produce a glyph string in device space. Glyph vectors we pass to it for
 * measuring or rendering should be in device space. The metrics which we
 * get back from the font will be in device space. The contents of the
 * global glyph image cache will be in device space.
 *
 *
 * Cairo's public view
 * -------------------
 *
 * Since the values entering and leaving via public API calls are in user
 * space, the gstate functions typically need to multiply argumens by the
 * CTM (for user-input glyph vectors), and return values by the CTM inverse
 * (for font responses such as metrics or glyph vectors).
 *
 */

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate)
{
    if (!gstate->font_face) {
	cairo_font_face_t *font_face;

	font_face = _cairo_toy_font_face_create (CAIRO_FONT_FAMILY_DEFAULT,
						 CAIRO_FONT_SLANT_DEFAULT,
						 CAIRO_FONT_WEIGHT_DEFAULT);
	if (font_face->status)
	    return font_face->status;
	else
	    gstate->font_face = font_face;
    }
    
    return CAIRO_STATUS_SUCCESS;
}
    
static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_font_options_t options;
    
    if (gstate->scaled_font)
	return CAIRO_STATUS_SUCCESS;
    
    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    cairo_surface_get_font_options (gstate->target, &options);
    cairo_font_options_merge (&options, &gstate->font_options);
    
    gstate->scaled_font = cairo_scaled_font_create (gstate->font_face,
						    &gstate->font_matrix,
						    &gstate->ctm,
						    &options);
    
    if (!gstate->scaled_font)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_get_font_extents (cairo_gstate_t *gstate, 
				cairo_font_extents_t *extents)
{
    cairo_status_t status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_extents (gstate->scaled_font, extents);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_text_to_glyphs (cairo_gstate_t *gstate, 
			      const char     *utf8,
			      double	      x,
			      double	      y,
			      cairo_glyph_t **glyphs,
			      int	     *num_glyphs)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    status = _cairo_scaled_font_text_to_glyphs (gstate->scaled_font, x, y,
						utf8, glyphs, num_glyphs);

    if (status || !glyphs || !num_glyphs || !(*glyphs) || !(num_glyphs))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_face (cairo_gstate_t    *gstate, 
			     cairo_font_face_t *font_face)
{
    if (font_face && font_face->status)
	return font_face->status;
    
    if (font_face != gstate->font_face) {
	cairo_font_face_destroy (gstate->font_face);
	gstate->font_face = cairo_font_face_reference (font_face);
    }

    _cairo_gstate_unset_scaled_font (gstate);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     cairo_glyph_t *glyphs, 
			     int num_glyphs,
			     cairo_text_extents_t *extents)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_glyph_extents (gstate->scaled_font,
				     glyphs, num_glyphs,
				     extents);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate, 
			   cairo_glyph_t *glyphs, 
			   int num_glyphs)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern;
    cairo_glyph_t *transformed_glyphs;
    int i;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &transformed_glyphs[i].x, 
				       &transformed_glyphs[i].y);
    }

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);

    status = _cairo_surface_show_glyphs (gstate->target,
					 gstate->op,
					 &source_pattern.base,
					 transformed_glyphs,
					 num_glyphs,
					 gstate->scaled_font);

    _cairo_pattern_fini (&source_pattern.base);
    free (transformed_glyphs);

    return status;
}

cairo_status_t
_cairo_gstate_glyph_path (cairo_gstate_t     *gstate,
			  cairo_glyph_t	     *glyphs, 
			  int		      num_glyphs,
			  cairo_path_fixed_t *path)
{
    cairo_status_t status;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &(transformed_glyphs[i].x), 
				       &(transformed_glyphs[i].y));
    }

    status = _cairo_scaled_font_glyph_path (gstate->scaled_font,
					    transformed_glyphs, num_glyphs,
					    path);

    free (transformed_glyphs);
    return status;
}

cairo_status_t
_cairo_gstate_set_antialias (cairo_gstate_t *gstate,
			     cairo_antialias_t antialias)
{
    gstate->antialias = antialias;

    return CAIRO_STATUS_SUCCESS;
}

cairo_antialias_t
_cairo_gstate_get_antialias (cairo_gstate_t *gstate)
{
    return gstate->antialias;
}

