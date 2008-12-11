/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 *      Vladimir Vukicevic <vladimir@pobox.com>
 *      Søren Sandmann <sandmann@daimi.au.dk>
 */

#include "cairoint.h"

const cairo_region_t _cairo_region_nil = {
    CAIRO_STATUS_NO_MEMORY,		/* status */
};

cairo_region_t *
_cairo_region_create (void)
{
    cairo_region_t *region = _cairo_malloc (sizeof (cairo_region_t));

    if (!region)
	return (cairo_region_t *)&_cairo_region_nil;

    region->status = CAIRO_STATUS_SUCCESS;

    pixman_region32_init (&region->rgn);

    return region;
}

cairo_region_t *
_cairo_region_create_rect (cairo_rectangle_int_t *rect)
{
    cairo_region_t *region = _cairo_malloc (sizeof (cairo_region_t));

    if (!region)
	return (cairo_region_t *)&_cairo_region_nil;
    
    region->status = CAIRO_STATUS_SUCCESS;
    
    pixman_region32_init_rect (&region->rgn,
			       rect->x, rect->y,
			       rect->width, rect->height);

    return region;
}

cairo_region_t *
_cairo_region_create_boxes (cairo_box_int_t *boxes,
			    int count)
{
    pixman_box32_t stack_pboxes[CAIRO_STACK_ARRAY_LENGTH (pixman_box32_t)];
    pixman_box32_t *pboxes = stack_pboxes;
    cairo_region_t *region;
    int i;

    region = _cairo_malloc (sizeof (cairo_region_t));

    if (!region)
	return (cairo_region_t *)&_cairo_region_nil;

    if (count > ARRAY_LENGTH (stack_pboxes)) {
	pboxes = _cairo_malloc_ab (count, sizeof (pixman_box32_t));

	if (unlikely (pboxes == NULL)) {
	    free (region);
	    return (cairo_region_t *)&_cairo_region_nil;
	}
    }

    for (i = 0; i < count; i++) {
	pboxes[i].x1 = boxes[i].p1.x;
	pboxes[i].y1 = boxes[i].p1.y;
	pboxes[i].x2 = boxes[i].p2.x;
	pboxes[i].y2 = boxes[i].p2.y;
    }

    if (! pixman_region32_init_rects (&region->rgn, pboxes, count)) {
	free (region);

	region = (cairo_region_t *)&_cairo_region_nil;
    }
    
    if (pboxes != stack_pboxes)
	free (pboxes);

    return region;
}

void
_cairo_region_destroy (cairo_region_t *region)
{
    if (region->status)
	return;
    
    pixman_region32_fini (&region->rgn);
    free (region);
}

cairo_region_t *
_cairo_region_copy (cairo_region_t *original)
{
    cairo_region_t *copy;
    
    if (original->status)
	return (cairo_region_t *)&_cairo_region_nil;

    copy = _cairo_region_create ();
    if (!copy)
	return (cairo_region_t *)&_cairo_region_nil;

    if (!pixman_region32_copy (&copy->rgn, &original->rgn)) {
	_cairo_region_destroy (copy);

	return (cairo_region_t *)&_cairo_region_nil;
    }

    return CAIRO_STATUS_SUCCESS;
}

int
_cairo_region_num_boxes (cairo_region_t *region)
{
    if (region->status)
	return 0;
    
    return pixman_region32_n_rects (&region->rgn);
}

cairo_private void
_cairo_region_get_box (cairo_region_t *region,
		       int nth_box,
		       cairo_box_int_t *box)
{
    pixman_box32_t *pbox;

    if (region->status)
	return;
    
    pbox = pixman_region32_rectangles (&region->rgn, NULL) + nth_box;

    box->p1.x = pbox->x1;
    box->p1.y = pbox->y1;
    box->p2.x = pbox->x2;
    box->p2.y = pbox->y2;
}

/**
 * _cairo_region_get_extents:
 * @region: a #cairo_region_t
 * @rect: rectangle into which to store the extents
 *
 * Gets the bounding box of a region as a #cairo_rectangle_int_t
 **/
void
_cairo_region_get_extents (cairo_region_t *region,
			   cairo_rectangle_int_t *extents)
{
    pixman_box32_t *pextents;

    if (region->status || !extents)
	return;

    pextents = pixman_region32_extents (&region->rgn);

    extents->x = pextents->x1;
    extents->y = pextents->y1;
    extents->width = pextents->x2 - pextents->x1;
    extents->height = pextents->y2 - pextents->y1;
}

cairo_status_t
_cairo_region_status (cairo_region_t *region)
{
    return region->status;
}

void
_cairo_region_clear (cairo_region_t *region)
{
    if (region->status)
	return;

    pixman_region32_fini (&region->rgn);
    pixman_region32_init (&region->rgn);
}

cairo_status_t
_cairo_region_subtract (cairo_region_t *dst, cairo_region_t *other)
{
    if (dst->status)
	return dst->status;

    if (other->status)
	return other->status;
    
    if (!pixman_region32_subtract (&dst->rgn, &dst->rgn, &other->rgn))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_region_intersect (cairo_region_t *dst, cairo_region_t *other)
{
    if (dst->status)
	return dst->status;

    if (other->status)
	return other->status;
    
    if (!pixman_region32_intersect (&dst->rgn, &dst->rgn, &other->rgn))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_region_union_rect (cairo_region_t *dst,
			  cairo_rectangle_int_t *rect)
{
    if (!pixman_region32_union_rect (&dst->rgn, &dst->rgn,
				     rect->x, rect->y,
				     rect->width, rect->height))
    {
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t
_cairo_region_not_empty (cairo_region_t *region)
{
    if (region->status)
	return FALSE;
    
    return (cairo_bool_t) pixman_region32_not_empty (&region->rgn);
}

void
_cairo_region_translate (cairo_region_t *region,
			 int x, int y)
{
    if (region->status)
	return;
    
    pixman_region32_translate (&region->rgn, x, y);
}

pixman_region_overlap_t
_cairo_region_contains_rectangle (cairo_region_t *region,
				  const cairo_rectangle_int_t *rect)
{
    pixman_box32_t pbox;

    if (region->status)
	return PIXMAN_REGION_OUT;
    
    pbox.x1 = rect->x;
    pbox.y1 = rect->y;
    pbox.x2 = rect->x + rect->width;
    pbox.y2 = rect->y + rect->height;

    return pixman_region32_contains_rectangle (&region->rgn, &pbox);
}
