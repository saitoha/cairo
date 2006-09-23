/*
 * Copyright © 2004 Carl Worth
 * Copyright © 2006 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Carl Worth
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

/* Provide definitions for standalone compilation */
#include "cairoint.h"

#include "cairo-skiplist-private.h"

#define CAIRO_BO_GUARD_BITS 2

typedef cairo_point_t cairo_bo_point32_t;

typedef struct _cairo_bo_point128 {
    cairo_int128_t x;
    cairo_int128_t y;
} cairo_bo_point128_t;

typedef struct _cairo_bo_point_quorem128 {
    cairo_quorem128_t x;
    cairo_quorem128_t y;
} cairo_bo_point_quorem128_t;

typedef struct _cairo_bo_edge cairo_bo_edge_t;
typedef struct _sweep_line_elt sweep_line_elt_t;

struct _cairo_bo_edge {
    cairo_bo_point32_t top;
    cairo_bo_point32_t middle;
    cairo_bo_point32_t bottom;
    cairo_bool_t reversed;
    cairo_bo_edge_t *prev;
    cairo_bo_edge_t *next;
    sweep_line_elt_t *sweep_line_elt;
};

struct _sweep_line_elt {
    cairo_bo_edge_t *edge;
    skip_elt_t elt;
};

#define SKIP_ELT_TO_EDGE_ELT(elt)	SKIP_LIST_ELT_TO_DATA (sweep_line_elt_t, (elt))
#define SKIP_ELT_TO_EDGE(elt) 		(SKIP_ELT_TO_EDGE_ELT (elt)->edge)

typedef enum {
    CAIRO_BO_STATUS_INTERSECTION,
    CAIRO_BO_STATUS_PARALLEL,
    CAIRO_BO_STATUS_NO_INTERSECTION
} cairo_bo_status_t;

typedef enum {
    CAIRO_BO_EVENT_TYPE_START,
    CAIRO_BO_EVENT_TYPE_STOP,
    CAIRO_BO_EVENT_TYPE_INTERSECTION
} cairo_bo_event_type_t;

typedef struct _cairo_bo_event {
    cairo_bo_event_type_t type;
    cairo_bo_edge_t *e1;
    cairo_bo_edge_t *e2;
    cairo_bo_point32_t point;
    skip_elt_t elt;
} cairo_bo_event_t;

#define SKIP_ELT_TO_EVENT(elt) SKIP_LIST_ELT_TO_DATA (cairo_bo_event_t, (elt))

typedef skip_list_t cairo_bo_event_queue_t;

/* This structure extends skip_list_t, which must come first. */
typedef struct _cairo_bo_sweep_line {
    skip_list_t active_edges;
    cairo_bo_edge_t *head;
    cairo_bo_edge_t *tail;
    int32_t current_y;
} cairo_bo_sweep_line_t;

static int
_cairo_bo_point32_compare (cairo_bo_point32_t *a,
			   cairo_bo_point32_t *b)
{
    if (a->y > b->y)
	return 1;
    else if (a->y < b->y)
	return -1;

    if (a->x > b->x)
	return 1;
    else if (a->x < b->x)
	return -1;

    return 0;
}

/* Compare the slope of a to the slope of b, returning 1, 0, -1 if the
 * slope a is respectively greater than, equal to, or less than the
 * slope of b.
 *
 * For each edge, consider the direction vector formed from:
 *
 *	top -> bottom
 *
 * which is:
 *
 *	(dx, dy) = (bottom.x - top.x, bottom.y - top.y)
 *
 * We then define the slope of each edge as dx/dy, (which is the
 * inverse of the slope typically used in math instruction). We never
 * compute a slope directly as the value approaches infinity, but we
 * can derive a slope comparison without division as follows, (where
 * the ? represents our compare operator).
 *
 * 1.	   slope(a) ? slope(b)
 * 2.	    adx/ady ? bdx/bdy
 * 3.	(adx * bdy) ? (bdx * ady)
 *
 * Note that from step 2 to step 3 there is no change needed in the
 * sign of the result since both ady and bdy are guaranteed to be
 * greater than or equal to 0.
 *
 * When using this slope comparison to sort edges, some care is needed
 * when interpreting the results. Since the slope compare operates on
 * distance vectors from top to bottom it gives a correct left to
 * right sort for edges that have a common top point, (such as two
 * edges with start events at the same location). On the other hand,
 * the sense of the result will be exactly reversed for two edges that
 * have a common stop point.
 */
static int
_slope_compare (cairo_bo_edge_t *a,
		cairo_bo_edge_t *b)
{
    /* XXX: We're assuming here that dx and dy will still fit in 32
     * bits. That's not true in general as there could be overflow. We
     * should prevent that before the tessellation algorithm
     * begins.
     */
    int32_t adx = a->bottom.x - a->top.x;
    int32_t ady = a->bottom.y - a->top.y;

    int32_t bdx = b->bottom.x - b->top.x;
    int32_t bdy = b->bottom.y - b->top.y;

    int64_t adx_bdy = _cairo_int32x32_64_mul (adx, bdy);
    int64_t bdx_ady = _cairo_int32x32_64_mul (bdx, ady);

    /* if (adx * bdy > bdx * ady) */
    if (_cairo_int64_gt (adx_bdy, bdx_ady))
	return 1;

    /* if (adx * bdy < bdx * ady) */
    if (_cairo_int64_lt (adx_bdy, bdx_ady))
	return -1;

    return 0;
}

static cairo_quorem64_t
edge_x_for_y (cairo_bo_edge_t *edge,
	      int32_t y)
{
    /* XXX: We're assuming here that dx and dy will still fit in 32
     * bits. That's not true in general as there could be overflow. We
     * should prevent that before the tessellation algorithm
     * begins.
     */
    int32_t dx = edge->bottom.x - edge->top.x;
    int32_t dy = edge->bottom.y - edge->top.y;
    int64_t numerator;
    cairo_quorem64_t quorem;

    if (dy == 0) {
	quorem.quo = _cairo_int32_to_int64 (edge->top.x);
	quorem.rem = 0;
	return quorem;
    }

    /* edge->top.x + (y - edge->top.y) * dx / dy */
    /* Multiplication followed by division means the guard bits cancel out. */
    numerator = _cairo_int32x32_64_mul ((y - edge->top.y), dx);
    quorem = _cairo_int64_divrem (numerator, dy);
    quorem.quo += edge->top.x;

    return quorem;
}

static int
_cairo_bo_sweep_line_compare_edges (cairo_bo_sweep_line_t	*sweep_line,
				    cairo_bo_edge_t		*a,
				    cairo_bo_edge_t		*b)
{
    cairo_quorem64_t ax = edge_x_for_y (a, sweep_line->current_y);
    cairo_quorem64_t bx = edge_x_for_y (b, sweep_line->current_y);
    int cmp;

    if (a == b)
	return 0;

    if (ax.quo > bx.quo)
	return 1;
    else if (ax.quo < bx.quo)
	return -1;

    /* Quotients are identical, test remainder. */
    if (ax.rem > bx.rem)
	return 1;
    else if (ax.rem < bx.rem)
	return -1;

    /* The two edges intersect exactly at y, so fall back on slope
     * comparison. We know that this compare_edges function will be
     * called only when starting a new edge, (not when stopping an
     * edge), so we don't have to worry about conditionally inverting
     * the sense of _slope_compare. */
    cmp = _slope_compare (a, b);
    if (cmp)
	return cmp;

    /* We've got two collinear edges now. */

    /* Since we're dealing with start events, prefer comparing top
     * edges before bottom edges. */
    cmp = _cairo_bo_point32_compare (&a->top, &b->top);
    if (cmp)
	return cmp;

    cmp = _cairo_bo_point32_compare (&a->bottom, &b->bottom);
    if (cmp)
	return cmp;

    /* Finally, we've got two identical edges. Let's finally
     * discriminate by a simple pointer comparison, (which works only
     * because we "know" the edges are all in a single array and don't
     * move. */
    if (a > b)
	return 1;
    else
	return -1;
}

static int
_sweep_line_elt_compare (void	*list,
			 void	*a,
			 void	*b)
{
    cairo_bo_sweep_line_t *sweep_line = list;
    sweep_line_elt_t *edge_elt_a = a;
    sweep_line_elt_t *edge_elt_b = b;

    return _cairo_bo_sweep_line_compare_edges (sweep_line,
					       edge_elt_a->edge,
					       edge_elt_b->edge);
}

static int
cairo_bo_event_compare (cairo_bo_event_t *a,
			cairo_bo_event_t *b)
{
    int cmp;

    /* The major motion of the sweep line is vertical (top-to-bottom),
     * and the minor motion is horizontal (left-to-right), dues to the
     * infinitesimal tilt rule.
     *
     * Our point comparison function respects these rules.
     */
    cmp = _cairo_bo_point32_compare (&a->point, &b->point);
    if (cmp)
	return cmp;

    /* The events share a common point, so further discrimination is
     * determined by the event type. Due to the infinitesimal
     * shortening rule, stop events come first, then intersection
     * events, then start events.
     */
    if (a->type != b->type) {
	if (a->type == CAIRO_BO_EVENT_TYPE_STOP)
	    return -1;
	if (a->type == CAIRO_BO_EVENT_TYPE_START)
	    return 1;

	if (b->type == CAIRO_BO_EVENT_TYPE_STOP)
	    return 1;
	if (b->type == CAIRO_BO_EVENT_TYPE_START)
	    return -1;
    }

    /* At this stage we are looking at two events of the same type at
     * the same point. The final sort key is a slope comparison. We
     * need a different sense for start and stop events based on the
     * shortening rule.
     *
     * NOTE: Fortunately, we get to ignore errors in the relative
     * ordering of intersection events. This means we don't even have
     * to look at e2 here, nor worry about which sense of the slope
     * comparison test is used for intersection events.
     */
    cmp = _slope_compare (a->e1, b->e1);
    if (cmp) {
	if (a->type == CAIRO_BO_EVENT_TYPE_START)
	    return cmp;
	else
	    return - cmp;
    }

    /* As a final discrimination, look at the opposite point. This
     * leaves ambiguities only for identical edges.
     */
    if (a->type == CAIRO_BO_EVENT_TYPE_START)
	return _cairo_bo_point32_compare (&b->e1->bottom,
					  &a->e1->bottom);
    else if (a->type == CAIRO_BO_EVENT_TYPE_STOP)
	return _cairo_bo_point32_compare (&a->e1->top,
					  &b->e1->top);
    else { /* CAIRO_BO_EVENT_TYPE_INTERSECT */
	/* For two intersection events at the identical point, we
	 * don't care what order they sort in, but we do care that we
	 * have a stable sort. In particular intersections between
	 * different pairs of edges must never return 0. */
	cmp = _cairo_bo_point32_compare (&a->e2->top, &b->e2->top);
	if (cmp)
	    return cmp;
	cmp = _cairo_bo_point32_compare (&a->e2->bottom, &b->e2->bottom);
	if (cmp)
	    return cmp;
	cmp = _cairo_bo_point32_compare (&a->e1->top, &b->e1->top);
	if (cmp)
	    return cmp;
	return _cairo_bo_point32_compare (&a->e1->bottom, &b->e1->bottom);
    }
}

static inline int
cairo_bo_event_compare_abstract (void		*list,
				 void	*a,
				 void	*b)
{
    cairo_bo_event_t *event_a = a;
    cairo_bo_event_t *event_b = b;

    return cairo_bo_event_compare (event_a, event_b);
}

static inline cairo_int64_t
det32_64 (int32_t a,
	  int32_t b,
	  int32_t c,
	  int32_t d)
{
    cairo_int64_t ad;
    cairo_int64_t bc;

    /* det = a * d - b * c */
    ad = _cairo_int64_rsa (_cairo_int32x32_64_mul (a, d), CAIRO_BO_GUARD_BITS);
    bc = _cairo_int64_rsa (_cairo_int32x32_64_mul (b, c), CAIRO_BO_GUARD_BITS);

    return _cairo_int64_sub (ad, bc);
}

/* We don't shift right by CAIRO_BO_GUARD_BITS here, anticipating a
 * subsequent division. */
static inline cairo_int128_t
det64_128 (cairo_int64_t a,
	   cairo_int64_t b,
	   cairo_int64_t c,
	   cairo_int64_t d)
{
    cairo_int128_t ad;
    cairo_int128_t bc;

    /* det = a * d - b * c */
    ad = _cairo_int64x64_128_mul (a, d);
    bc = _cairo_int64x64_128_mul (b, c);

    return _cairo_int128_sub (ad, bc);
}

/* Compute the intersection of two lines as defined by two edges. The
 * result is provided as a coordinate pair of 128-bit integers.
 *
 * Returns CAIRO_BO_STATUS_INTERSECTION if there is an intersection or
 * CAIRO_BO_STATUS_PARALLEL if the two lines are exactly parallel.
 */
static cairo_bo_status_t
intersect_lines (cairo_bo_edge_t		*a,
		 cairo_bo_edge_t		*b,
		 cairo_bo_point_quorem128_t	*intersection)
{
    cairo_int64_t a_det, b_det;

    /* XXX: We're assuming here that dx and dy will still fit in 32
     * bits. That's not true in general as there could be overflow. We
     * should prevent that before the tessellation algorithm
     * begins.
     */
    int32_t dx1 = a->top.x - a->bottom.x;
    int32_t dy1 = a->top.y - a->bottom.y;

    int32_t dx2 = b->top.x - b->bottom.x;
    int32_t dy2 = b->top.y - b->bottom.y;

    cairo_int64_t den_det = det32_64 (dx1, dy1, dx2, dy2);

    if (_cairo_int64_eq (den_det, 0))
	return CAIRO_BO_STATUS_PARALLEL;

    /* The expansion of guard bits in this multiplication is left to
     * be reduced again by the subsequent division. */
    a_det = det32_64 (a->top.x, a->top.y,
		      a->bottom.x, a->bottom.y);
    b_det = det32_64 (b->top.x, b->top.y,
		      b->bottom.x, b->bottom.y);

    /* x = det (a_det, dx1, b_det, dx2) / den_det */
    intersection->x = _cairo_int128_divrem (det64_128 (a_det, dx1,
						       b_det, dx2),
					    _cairo_int64_to_int128 (den_det));

    /* y = det (a_det, dy1, b_det, dy2) / den_det */
    intersection->y = _cairo_int128_divrem (det64_128 (a_det, dy1,
						       b_det, dy2),
					    _cairo_int64_to_int128 (den_det));

    return CAIRO_BO_STATUS_INTERSECTION;
}

static int
_cairo_quorem128_32_compare (cairo_quorem128_t	a,
			     int32_t		b)
{
    /* XXX: Converting up to a int128_t here is silly, but I'm lazy. */
    cairo_int128_t b_128 = _cairo_int32_to_int128 (b);
    cairo_int128_t zero = _cairo_int32_to_int128 (0);

    /* First compare the quotient */
    if (_cairo_int128_gt (a.quo, b_128))
	return 1;
    if (_cairo_int128_lt (a.quo, b_128))
	return -1;

    /* With quotient identical, if remainder is 0 then compare equal */
    if (_cairo_int128_eq (a.rem, zero))
	return 0;

    /* Otherwise, the non-zero remainder makes a > b */
    return 1;
}

/* Does the given edge contain the given point. The point must already
 * be known to be contained within the line determined by the edge,
 * (most likely the point results from an intersection of this edge
 * with another).
 *
 * If we had exact arithmetic, then this function would simply be a
 * matter of examining whether the y value of the point lies within
 * the range of y values of the edge. But since intersection points
 * are not exact due to being rounded to the nearest integer within
 * the available precision, we must also examine the x value of the
 * point.
 *
 * The definition of "contains" here is that the given intersection
 * point will be seen by the sweep line after the start event for the
 * given edge and before the stop event for the edge. See the comments
 * in the implementation for more details.
 */
static cairo_bool_t
_cairo_bo_edge_contains_point_quorem128 (cairo_bo_edge_t		*edge,
					 cairo_bo_point_quorem128_t	*point)
{
    int cmp_top, cmp_bottom;

    /* XXX: When running the actual algorithm, we don't actually need to
     * compare against edge->top at all here, since any intersection above
     * top is eliminated early via a slope comparison. We're leaving these
     * here for now only for the sake of the quadratic-time intersection
     * finder which needs them.
     */

    cmp_top = _cairo_quorem128_32_compare (point->y, edge->top.y);
    cmp_bottom = _cairo_quorem128_32_compare (point->y, edge->bottom.y);

    if (cmp_top < 0 || cmp_bottom > 0)
    {
	return FALSE;
    }

    if (cmp_top > 0 && cmp_bottom < 0)
    {
	return TRUE;
    }

    /* At this stage, the point lies on the same y value as either
     * edge->top or edge->bottom, so we have to examine the x value in
     * order to properly determine containment. */

    /* If the y value of the point is the same as the y value of the
     * top of the edge, then the x value of the point must be greater
     * to be considered as inside the edge. Similarly, if the y value
     * of the point is the same as the y value of the bottom of the
     * edge, then the x value of the point must be less to be
     * considered as inside. */

    if (cmp_top == 0)
	return (_cairo_quorem128_32_compare (point->x, edge->top.x) > 0);
    else /* cmp_bottom == 0 */
	return (_cairo_quorem128_32_compare (point->x, edge->bottom.x) < 0);
}

/* Compute the intersection of two edges. The result is provided as a
 * coordinate pair of 128-bit integers.
 *
 * Returns CAIRO_BO_STATUS_INTERSECTION if there is an intersection
 * that is within both edges, CAIRO_BO_STATUS_NO_INTERSECTION if the
 * intersection of the lines defined by the edges occurs outside of
 * one or both edges, and CAIRO_BO_STATUS_PARALLEL if the two edges
 * are exactly parallel.
 *
 * Note that when determining if a candidate intersection is "inside"
 * an edge, we consider both the infinitesimal shortening and the
 * infinitesimal tilt rules described by John Hobby. Specifically, if
 * the intersection is exactly the same as an edge point, it is
 * effectively outside (no intersection is returned). Also, if the
 * intersection point has the same
 */
static cairo_bo_status_t
_cairo_bo_edge_intersect (cairo_bo_edge_t	*a,
			  cairo_bo_edge_t	*b,
			  cairo_bo_point32_t	*intersection)
{
    cairo_bo_status_t status;
    cairo_bo_point_quorem128_t quorem;

    status = intersect_lines (a, b, &quorem);
    if (status)
	return status;

    if (! _cairo_bo_edge_contains_point_quorem128 (a, &quorem))
	return CAIRO_BO_STATUS_NO_INTERSECTION;

    if (! _cairo_bo_edge_contains_point_quorem128 (b, &quorem))
	return CAIRO_BO_STATUS_NO_INTERSECTION;

    /* Now that we've correctly compared the intersection point and
     * determined that it lies within the edge, then we know that we
     * no longer need any more bits of storage for the intersection
     * than we do for our edge coordinates. We also no longer need the
     * remainder from the division. */
    intersection->x = _cairo_int128_to_int32 (quorem.x.quo);
    intersection->y = _cairo_int128_to_int32 (quorem.y.quo);

    return CAIRO_BO_STATUS_INTERSECTION;
}

static void
_cairo_bo_event_init (cairo_bo_event_t		*event,
		      cairo_bo_event_type_t	 type,
		      cairo_bo_edge_t	*e1,
		      cairo_bo_edge_t	*e2,
		      cairo_bo_point32_t	 point)
{
    event->type = type;
    event->e1 = e1;
    event->e2 = e2;
    event->point = point;
}

static void
_cairo_bo_event_queue_insert (cairo_bo_event_queue_t *queue,
			      cairo_bo_event_t	     *event)
{
    /* Don't insert if there's already an equivalent intersection event in the queue. */
    if (event->type == CAIRO_BO_EVENT_TYPE_INTERSECTION &&
	skip_list_find (queue, event) != NULL)
    {
	return;
    }

    skip_list_insert (queue, event);
}

static void
_cairo_bo_event_queue_delete (cairo_bo_event_queue_t *queue,
			      cairo_bo_event_t	     *event)
{
    skip_list_delete (queue, event);
}

static void
_cairo_bo_event_queue_init (cairo_bo_event_queue_t	*event_queue,
			    cairo_bo_edge_t	*edges,
			    int				 num_edges)
{
    int i;
    cairo_bo_event_t event;

    skip_list_init (event_queue,
		    cairo_bo_event_compare_abstract,
		    sizeof (cairo_bo_event_t));

    for (i = 0; i < num_edges; i++) {
	/* We must not be given horizontal edges. */
	assert (edges[i].top.y != edges[i].bottom.y);

	/* We also must not be given any upside-down edges. */
	assert (_cairo_bo_point32_compare (&edges[i].top, &edges[i].bottom) < 0);

	/* Initialize "middle" to top */
	edges[i].middle = edges[i].top;

	_cairo_bo_event_init (&event,
			      CAIRO_BO_EVENT_TYPE_START,
			      &edges[i], NULL,
			      edges[i].top);

	_cairo_bo_event_queue_insert (event_queue, &event);

	_cairo_bo_event_init (&event,
			      CAIRO_BO_EVENT_TYPE_STOP,
			      &edges[i], NULL,
			      edges[i].bottom);

	_cairo_bo_event_queue_insert (event_queue, &event);
    }
}

static void
_cairo_bo_event_queue_insert_if_intersect_below_current_y (cairo_bo_event_queue_t	*event_queue,
							   cairo_bo_edge_t	*left,
							   cairo_bo_edge_t	*right)
{
    cairo_bo_status_t status;
    cairo_bo_point32_t intersection;
    cairo_bo_event_t event;

    if (left == NULL || right == NULL)
	return;

    /* The names "left" and "right" here are correct descriptions of
     * the order of the two edges within the active edge list. So if a
     * slope comparison also puts left less than right, then we know
     * that the intersection of these two segments has oalready
     * occurred before the current sweep line position. */
    if (_slope_compare (left, right) < 0)
	return;

    status = _cairo_bo_edge_intersect (left, right, &intersection);
    if (status == CAIRO_BO_STATUS_PARALLEL ||
	status == CAIRO_BO_STATUS_NO_INTERSECTION)
    {
	return;
    }

    _cairo_bo_event_init (&event,
			  CAIRO_BO_EVENT_TYPE_INTERSECTION,
			  left, right,
			  intersection);

    _cairo_bo_event_queue_insert (event_queue, &event);
}

static void
_cairo_bo_sweep_line_init (cairo_bo_sweep_line_t *sweep_line)
{
    skip_list_init (&sweep_line->active_edges,
		    _sweep_line_elt_compare,
		    sizeof (sweep_line_elt_t));
    sweep_line->head = NULL;
    sweep_line->tail = NULL;
    sweep_line->current_y = 0;
}

static void
_cairo_bo_sweep_line_insert (cairo_bo_sweep_line_t	*sweep_line,
			     cairo_bo_edge_t		*edge)
{
    skip_elt_t *next_elt;
    sweep_line_elt_t *sweep_line_elt;
    cairo_bo_edge_t **prev_of_next, **next_of_prev;

    sweep_line_elt = skip_list_insert (&sweep_line->active_edges, &edge);

    next_elt = sweep_line_elt->elt.next[0];
    if (next_elt)
	prev_of_next = & (SKIP_ELT_TO_EDGE (next_elt)->prev);
    else
	prev_of_next = &sweep_line->tail;

    if (*prev_of_next)
	next_of_prev = &(*prev_of_next)->next;
    else
	next_of_prev = &sweep_line->head;

    edge->prev = *prev_of_next;
    edge->next = *next_of_prev;
    *prev_of_next = edge;
    *next_of_prev = edge;

    edge->sweep_line_elt = sweep_line_elt;
}

static void
_cairo_bo_sweep_line_delete (cairo_bo_sweep_line_t	*sweep_line,
			     cairo_bo_edge_t	*edge)
{
    cairo_bo_edge_t **left_next, **right_prev;

    skip_list_delete_given (&sweep_line->active_edges, &edge->sweep_line_elt->elt);

    left_next = &sweep_line->head;
    if (edge->prev)
	left_next = &edge->prev->next;

    right_prev = &sweep_line->tail;
    if (edge->next)
	right_prev = &edge->next->prev;

    *left_next = edge->next;
    *right_prev = edge->prev;
}

static void
_cairo_bo_sweep_line_swap (cairo_bo_sweep_line_t	*sweep_line,
			   cairo_bo_edge_t		*left,
			   cairo_bo_edge_t		*right)
{
    sweep_line_elt_t *left_elt, *right_elt;
    cairo_bo_edge_t **before_left, **after_right;

    /* Within the skip list we can do the swap simply by swapping the
     * pointers to the edge elements and leaving all of the skip list
     * elements and pointers unchanged. */
    left_elt = left->sweep_line_elt;
    right_elt = SKIP_ELT_TO_EDGE_ELT (left_elt->elt.next[0]);

    left_elt->edge = right;
    right->sweep_line_elt = left_elt;

    right_elt->edge = left;
    left->sweep_line_elt = right_elt;

    /* Within the doubly-linked list of edges, there's a bit more
     * bookkeeping involved with the swap. */
    before_left = &sweep_line->head;
    if (left->prev)
	before_left = &left->prev->next;
    *before_left = right;

    after_right = &sweep_line->tail;
    if (right->next)
	after_right = &right->next->prev;
    *after_right = left;

    left->next = right->next;
    right->next = left;

    right->prev = left->prev;
    left->prev = right;
}

#define DEBUG_PRINT_STATE 0
#if DEBUG_PRINT_STATE
static void
_cairo_bo_edge_print (cairo_bo_edge_t *edge)
{
    printf ("(0x%x, 0x%x)-(0x%x, 0x%x)",
	    edge->top.x, edge->top.y,
	    edge->bottom.x, edge->bottom.y);
}

static void
_cairo_bo_event_print (cairo_bo_event_t *event)
{
    switch (event->type) {
    case CAIRO_BO_EVENT_TYPE_START:
	printf ("Start: ");
	break;
    case CAIRO_BO_EVENT_TYPE_STOP:
	printf ("Stop: ");
	break;
    case CAIRO_BO_EVENT_TYPE_INTERSECTION:
	printf ("Intersection: ");
	break;
    }
    printf ("(%d, %d)\t", event->point.x, event->point.y);
    _cairo_bo_edge_print (event->e1);
    if (event->type == CAIRO_BO_EVENT_TYPE_INTERSECTION) {
	printf (" X ");
	_cairo_bo_edge_print (event->e2);
    }
    printf ("\n");
}

static void
_cairo_bo_event_queue_print (cairo_bo_event_queue_t *queue)
{
    skip_elt_t *elt;
    cairo_bo_event_t *event;

    printf ("Event queue:\n");

    for (elt = queue->chains[0];
	 elt;
	 elt = elt->next[0])
    {
	event = SKIP_ELT_TO_EVENT (elt);
	_cairo_bo_event_print (event);
    }
}

static void
_cairo_bo_sweep_line_print (cairo_bo_sweep_line_t *sweep_line)
{
    cairo_bool_t first = TRUE;
    skip_elt_t *elt;
    cairo_bo_edge_t *edge;

    printf ("Sweep line (reversed):     ");

    for (edge = sweep_line->tail;
	 edge;
	 edge = edge->prev)
    {
	if (!first)
	    printf (", ");
	_cairo_bo_edge_print (edge);
	first = FALSE;
    }
    printf ("\n");


    printf ("Sweep line from edge list: ");
    first = TRUE;
    for (edge = sweep_line->head;
	 edge;
	 edge = edge->next)
    {
	if (!first)
	    printf (", ");
	_cairo_bo_edge_print (edge);
	first = FALSE;
    }
    printf ("\n");

    printf ("Sweep line from skip list: ");
    first = TRUE;
    for (elt = sweep_line->active_edges.chains[0];
	 elt;
	 elt = elt->next[0])
    {
	if (!first)
	    printf (", ");
	_cairo_bo_edge_print (SKIP_ELT_TO_EDGE (elt));
	first = FALSE;
    }
    printf ("\n");
}

static void
print_state (const char			*msg,
	     cairo_bo_event_queue_t	*event_queue,
	     cairo_bo_sweep_line_t	*sweep_line)
{
    printf ("%s\n", msg);
    _cairo_bo_event_queue_print (event_queue);
    _cairo_bo_sweep_line_print (sweep_line);
    printf ("\n");
}
#endif

static void
_cairo_bo_sweep_line_validate (cairo_bo_sweep_line_t *sweep_line)
{
    cairo_bo_edge_t *edge;
    skip_elt_t *elt;

    /* March through both the skip list's singly-linked list and the
     * sweep line's own list through pointers in the edges themselves
     * and make sure they agree at every point. */

    for (edge = sweep_line->head, elt = sweep_line->active_edges.chains[0];
	 edge && elt;
	 edge = edge->next, elt = elt->next[0])
    {
	if (SKIP_ELT_TO_EDGE (elt) != edge) {
	    fprintf (stderr, "*** Error: Sweep line fails to validate: Inconsistent data in the two lists.\n");
	    exit (1);
	}
    }

    if (edge || elt) {
	fprintf (stderr, "*** Error: Sweep line fails to validate: One list ran out before the other.\n");
	exit (1);
    }
}

static cairo_status_t
_active_edges_to_traps (cairo_bo_edge_t		*head,
			int32_t			 top,
			int32_t			 bottom,
			cairo_fill_rule_t	 fill_rule,
			cairo_traps_t		*traps)
{
    cairo_status_t status;
    int in_out = 0;
    cairo_bo_edge_t *edge;
    cairo_bo_point32_t left_top, left_bottom, right_top, right_bottom;

    for (edge = head; edge && edge->next; edge = edge->next) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING) {
	    if (edge->reversed)
		in_out++;
	    else
		in_out--;
	    if (in_out == 0)
		continue;
	} else {
	    in_out++;
	    if ((in_out & 1) == 0)
		continue;
	}
#if DEBUG_PRINT_STATE
	printf ("Adding trap 0x%x 0x%x: ", top, bottom);
	_cairo_bo_edge_print (edge);
	_cairo_bo_edge_print (edge->next);
#endif
	left_top.x = edge->middle.x >> CAIRO_BO_GUARD_BITS;
	left_top.y = edge->middle.y >> CAIRO_BO_GUARD_BITS;
	left_bottom.x = edge->bottom.x >> CAIRO_BO_GUARD_BITS;
	left_bottom.y = edge->bottom.y >> CAIRO_BO_GUARD_BITS;
	right_top.x = edge->next->middle.x >> CAIRO_BO_GUARD_BITS;
	right_top.y = edge->next->middle.y >> CAIRO_BO_GUARD_BITS;
	right_bottom.x = edge->next->bottom.x >> CAIRO_BO_GUARD_BITS;
	right_bottom.y = edge->next->bottom.y >> CAIRO_BO_GUARD_BITS;
	status = _cairo_traps_add_trap_from_points (traps,
						    top >> CAIRO_BO_GUARD_BITS,
						    bottom >> CAIRO_BO_GUARD_BITS,
						    left_top, left_bottom,
						    right_top, right_bottom);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Execute a single pass of the Bentley-Ottmann algorithm on edges,
 * generating trapezoids according to the fill_rule and appending them
 * to traps. */
static cairo_status_t
_cairo_bentley_ottmann_tessellate_bo_edges (cairo_bo_edge_t	*edges,
					    int			 num_edges,
					    cairo_fill_rule_t	 fill_rule,
					    cairo_traps_t	*traps,
					    int			*num_intersections)
{
    cairo_status_t status;
    int intersection_count = 0;
    cairo_bo_event_queue_t event_queue;
    cairo_bo_sweep_line_t sweep_line;
    skip_elt_t *elt;
    cairo_bo_event_t *event, event_saved;
    cairo_bo_edge_t *edge;
    cairo_bo_edge_t *left, *right;
    cairo_bo_edge_t *edge1, *edge2;

    int i;

    for (i = 0; i < num_edges; i++) {
	edge = &edges[i];
	edge->top.x <<= CAIRO_BO_GUARD_BITS;
	edge->top.y <<= CAIRO_BO_GUARD_BITS;
	edge->middle = edge->top;
	edge->bottom.x <<= CAIRO_BO_GUARD_BITS;
	edge->bottom.y <<= CAIRO_BO_GUARD_BITS;
    }

    _cairo_bo_event_queue_init (&event_queue, edges, num_edges);
    _cairo_bo_sweep_line_init (&sweep_line);

#if DEBUG_PRINT_STATE
    print_state ("After initializing", &event_queue, &sweep_line);
#endif

    while (1)
    {
	elt = event_queue.chains[0];
	if (elt == NULL)
	    break;
	event = SKIP_ELT_TO_EVENT (elt);

	if (event->point.y != sweep_line.current_y) {
	    status = _active_edges_to_traps (sweep_line.head,
					     sweep_line.current_y, event->point.y,
					     fill_rule, traps);
	    if (status)
		return status;

	    sweep_line.current_y = event->point.y;
	}

	event_saved = *event;
	_cairo_bo_event_queue_delete (&event_queue, event);
	event = &event_saved;

	switch (event->type) {
	case CAIRO_BO_EVENT_TYPE_START:
	    edge = event->e1;

	    _cairo_bo_sweep_line_insert (&sweep_line, edge);
	    /* Cache the insert position for use in pass 2.
	    event->e2 = Sortlist::prev (sweep_line, edge);
	    */

	    left = edge->prev;
	    right = edge->next;

	    _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue, left, edge);

	    _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue, edge, right);

#if DEBUG_PRINT_STATE
	    print_state ("After processing start", &event_queue, &sweep_line);
#endif
	    _cairo_bo_sweep_line_validate (&sweep_line);

	    break;
	case CAIRO_BO_EVENT_TYPE_STOP:
	    edge = event->e1;

	    left = edge->prev;
	    right = edge->next;

	    _cairo_bo_sweep_line_delete (&sweep_line, edge);

	    _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue, left, right);

#if DEBUG_PRINT_STATE
	    print_state ("After processing stop", &event_queue, &sweep_line);
#endif
	    _cairo_bo_sweep_line_validate (&sweep_line);

	    break;
	case CAIRO_BO_EVENT_TYPE_INTERSECTION:
	    edge1 = event->e1;
	    edge2 = event->e2;

	    /* skip this intersection if its edges are not adjacent */
	    if (edge2 != edge1->next)
		break;

	    intersection_count++;

	    edge1->middle = event->point;
	    edge2->middle = event->point;

	    left = edge1->prev;
	    right = edge2->next;

	    _cairo_bo_sweep_line_swap (&sweep_line, edge1, edge2);

	    /* after the swap e2 is left of e1 */

	    _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue,
								       left, edge2);

	    _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue,
								       edge1, right);

#if DEBUG_PRINT_STATE
	    print_state ("After processing intersection", &event_queue, &sweep_line);
#endif
	    _cairo_bo_sweep_line_validate (&sweep_line);

	    break;
	}
    }

    *num_intersections = intersection_count;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_polygon (cairo_traps_t	*traps,
					   cairo_polygon_t	*polygon,
					   cairo_fill_rule_t	 fill_rule)
{
    int intersections;
    cairo_status_t status;
    cairo_bo_edge_t *edges;
    int i;

    edges = malloc (polygon->num_edges * sizeof (cairo_bo_edge_t));
    if (edges == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < polygon->num_edges; i++) {
	edges[i].top = polygon->edges[i].edge.p1;
	edges[i].middle = edges[i].top;
	edges[i].bottom = polygon->edges[i].edge.p2;
	/* XXX: The 'clockWise' name that cairo_polygon_t uses is
	 * totally bogus. It's really a (negated!) description of
	 * whether the edge is reversed. */
	edges[i].reversed = (! polygon->edges[i].clockWise);
	edges[i].prev = NULL;
	edges[i].next = NULL;
	edges[i].sweep_line_elt = NULL;
    }

    /* XXX: This would be the convenient place to throw in multiple
     * passes of the Bentley-Ottmann algorithm. It would merely
     * require storing the results of each pass into a temporary
     * cairo_traps_t. */
    status = _cairo_bentley_ottmann_tessellate_bo_edges (edges, polygon->num_edges,
							 fill_rule, traps, &intersections);

    free (edges);

    return status;
}

#if 0
static cairo_bool_t
edges_have_an_intersection_quadratic (cairo_bo_edge_t	*edges,
				      int		 num_edges)

{
    int i, j;
    cairo_bo_edge_t *a, *b;
    cairo_bo_point32_t intersection;
    cairo_bo_status_t status;

    /* We must not be given any upside-down edges. */
    for (i = 0; i < num_edges; i++) {
	assert (_cairo_bo_point32_compare (&edges[i].top, &edges[i].bottom) < 0);
	edges[i].top.x <<= CAIRO_BO_GUARD_BITS;
	edges[i].top.y <<= CAIRO_BO_GUARD_BITS;
	edges[i].bottom.x <<= CAIRO_BO_GUARD_BITS;
	edges[i].bottom.y <<= CAIRO_BO_GUARD_BITS;
    }

    for (i = 0; i < num_edges; i++) {
	for (j = 0; j < num_edges; j++) {
	    if (i == j)
		continue;

	    a = &edges[i];
	    b = &edges[j];

	    status = _cairo_bo_edge_intersect (a, b, &intersection);
	    if (status == CAIRO_BO_STATUS_PARALLEL ||
		status == CAIRO_BO_STATUS_NO_INTERSECTION)
	    {
		continue;
	    }

	    printf ("Found intersection (%d,%d) between (%d,%d)-(%d,%d) and (%d,%d)-(%d,%d)\n",
		    intersection.x,
		    intersection.y,
		    a->top.x, a->top.y,
		    a->bottom.x, a->bottom.y,
		    b->top.x, b->top.y,
		    b->bottom.x, b->bottom.y);

	    return TRUE;
	}
    }
    return FALSE;
}

#define TEST_MAX_EDGES 10

typedef struct test {
    const char *name;
    const char *description;
    int num_edges;
    cairo_bo_edge_t edges[TEST_MAX_EDGES];
} test_t;

static test_t
tests[] = {
    {
	"3 near misses",
	"3 edges all intersecting very close to each other",
	3,
	{
	    { { 4, 2}, {0, 0}, { 9, 9}, NULL, NULL },
	    { { 7, 2}, {0, 0}, { 2, 3}, NULL, NULL },
	    { { 5, 2}, {0, 0}, { 1, 7}, NULL, NULL }
	}
    },
    {
	"inconsistent data",
	"Derived from random testing---was leading to skip list and edge list disagreeing.",
	2,
	{
	    { { 2, 3}, {0, 0}, { 8, 9}, NULL, NULL },
	    { { 2, 3}, {0, 0}, { 6, 7}, NULL, NULL }
	}
    },
    {
	"failed sort",
	"A test derived from random testing that leads to an inconsistent sort --- looks like we just can't attempt to validate the sweep line with edge_compare?",
	3,
	{
	    { { 6, 2}, {0, 0}, { 6, 5}, NULL, NULL },
	    { { 3, 5}, {0, 0}, { 5, 6}, NULL, NULL },
	    { { 9, 2}, {0, 0}, { 5, 6}, NULL, NULL },
	}
    },
    {
	"minimal-intersection",
	"Intersection of a two from among the smallest possible edges.",
	2,
	{
	    { { 0, 0}, {0, 0}, { 1, 1}, NULL, NULL },
	    { { 1, 0}, {0, 0}, { 0, 1}, NULL, NULL }
	}
    },
    {
	"simple",
	"A simple intersection of two edges at an integer (2,2).",
	2,
	{
	    { { 1, 1}, {0, 0}, { 3, 3}, NULL, NULL },
	    { { 2, 1}, {0, 0}, { 2, 3}, NULL, NULL }
	}
    },
    {
	"bend-to-horizontal",
	"With intersection truncation one edge bends to horizontal",
	2,
	{
	    { { 9, 1}, {0, 0}, {3, 7}, NULL, NULL },
	    { { 3, 5}, {0, 0}, {9, 9}, NULL, NULL }
	}
    }
};

/*
    {
	"endpoint",
	"An intersection that occurs at the endpoint of a segment.",
	{
	    { { 4, 6}, { 5, 6}, NULL, { { NULL }} },
	    { { 4, 5}, { 5, 7}, NULL, { { NULL }} },
	    { { 0, 0}, { 0, 0}, NULL, { { NULL }} },
	}
    }
    {
	name = "overlapping",
	desc = "Parallel segments that share an endpoint, with different slopes.",
	edges = {
	    { top = { x = 2, y = 0}, bottom = { x = 1, y = 1}},
	    { top = { x = 2, y = 0}, bottom = { x = 0, y = 2}},
	    { top = { x = 0, y = 3}, bottom = { x = 1, y = 3}},
	    { top = { x = 0, y = 3}, bottom = { x = 2, y = 3}},
	    { top = { x = 0, y = 4}, bottom = { x = 0, y = 6}},
	    { top = { x = 0, y = 5}, bottom = { x = 0, y = 6}}
	}
    },
    {
	name = "hobby_stage_3",
	desc = "A particularly tricky part of the 3rd stage of the 'hobby' test below.",
	edges = {
	    { top = { x = -1, y = -2}, bottom = { x =  4, y = 2}},
	    { top = { x =  5, y =  3}, bottom = { x =  9, y = 5}},
	    { top = { x =  5, y =  3}, bottom = { x =  6, y = 3}},
	}
    },
    {
	name = "hobby",
	desc = "Example from John Hobby's paper. Requires 3 passes of the iterative algorithm.",
	edges = {
	    { top = { x =   0, y =   0}, bottom = { x =   9, y =   5}},
	    { top = { x =   0, y =   0}, bottom = { x =  13, y =   6}},
	    { top = { x =  -1, y =  -2}, bottom = { x =   9, y =   5}}
	}
    },
    {
	name = "slope",
	desc = "Edges with same start/stop points but different slopes",
	edges = {
	    { top = { x = 4, y = 1}, bottom = { x = 6, y = 3}},
	    { top = { x = 4, y = 1}, bottom = { x = 2, y = 3}},
	    { top = { x = 2, y = 4}, bottom = { x = 4, y = 6}},
	    { top = { x = 6, y = 4}, bottom = { x = 4, y = 6}}
	}
    },
    {
	name = "horizontal",
	desc = "Test of a horizontal edge",
	edges = {
	    { top = { x = 1, y = 1}, bottom = { x = 6, y = 6}},
	    { top = { x = 2, y = 3}, bottom = { x = 5, y = 3}}
	}
    },
    {
	name = "vertical",
	desc = "Test of a vertical edge",
	edges = {
	    { top = { x = 5, y = 1}, bottom = { x = 5, y = 7}},
	    { top = { x = 2, y = 4}, bottom = { x = 8, y = 5}}
	}
    },
    {
	name = "congruent",
	desc = "Two overlapping edges with the same slope",
	edges = {
	    { top = { x = 5, y = 1}, bottom = { x = 5, y = 7}},
	    { top = { x = 5, y = 2}, bottom = { x = 5, y = 6}},
	    { top = { x = 2, y = 4}, bottom = { x = 8, y = 5}}
	}
    },
    {
	name = "multi",
	desc = "Several segments with a common intersection point",
	edges = {
	    { top = { x = 1, y = 2}, bottom = { x = 5, y = 4} },
	    { top = { x = 1, y = 1}, bottom = { x = 5, y = 5} },
	    { top = { x = 2, y = 1}, bottom = { x = 4, y = 5} },
	    { top = { x = 4, y = 1}, bottom = { x = 2, y = 5} },
	    { top = { x = 5, y = 1}, bottom = { x = 1, y = 5} },
	    { top = { x = 5, y = 2}, bottom = { x = 1, y = 4} }
	}
    }
};
*/

static int
run_test (const char		*test_name,
          cairo_bo_edge_t	*test_edges,
          int			 num_edges)
{
    int i, intersections, passes;
    cairo_bo_edge_t *edges;
    cairo_array_t intersected_edges;

    printf ("Testing: %s\n", test_name);

    _cairo_array_init (&intersected_edges, sizeof (cairo_bo_edge_t));

    intersections = _cairo_bentley_ottmann_intersect_edges (test_edges, num_edges, &intersected_edges);
    if (intersections)
	printf ("Pass 1 found %d intersections:\n", intersections);


    /* XXX: Multi-pass Bentley-Ottmmann. Preferable would be to add a
     * pass of Hobby's tolerance-square algorithm instead. */
    passes = 1;
    while (intersections) {
	int num_edges = _cairo_array_num_elements (&intersected_edges);
	passes++;
	edges = malloc (num_edges * sizeof (cairo_bo_edge_t));
	assert (edges != NULL);
	memcpy (edges, _cairo_array_index (&intersected_edges, 0), num_edges * sizeof (cairo_bo_edge_t));
	_cairo_array_fini (&intersected_edges);
	_cairo_array_init (&intersected_edges, sizeof (cairo_bo_edge_t));
	intersections = _cairo_bentley_ottmann_intersect_edges (edges, num_edges, &intersected_edges);
	free (edges);

	if (intersections){
	    printf ("Pass %d found %d remaining intersections:\n", passes, intersections);
	} else {
	    if (passes > 3)
		for (i = 0; i < passes; i++)
		    printf ("*");
	    printf ("No remainining intersections found after pass %d\n", passes);
	}
    }

    if (edges_have_an_intersection_quadratic (_cairo_array_index (&intersected_edges, 0),
					      _cairo_array_num_elements (&intersected_edges)))
	printf ("*** FAIL ***\n");
    else
	printf ("PASS\n");

    _cairo_array_fini (&intersected_edges);

    return 0;
}

#define MAX_RANDOM 300

int
main (void)
{
    char random_name[] = "random-XX";
    static cairo_bo_edge_t random_edges[MAX_RANDOM], *edge;
    unsigned int i, num_random;
    test_t *test;

    for (i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
	test = &tests[i];
	run_test (test->name, test->edges, test->num_edges);
    }

    for (num_random = 0; num_random < MAX_RANDOM; num_random++) {
	srand (0);
	for (i = 0; i < num_random; i++) {
	    do {
		edge = &random_edges[i];
		edge->top.x = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		edge->top.y = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		edge->bottom.x = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		edge->bottom.y = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		if (edge->top.y > edge->bottom.y) {
		    int32_t tmp = edge->top.y;
		    edge->top.y = edge->bottom.y;
		    edge->bottom.y = tmp;
		}
	    } while (edge->top.y == edge->bottom.y);
	}

	sprintf (random_name, "random-%02d", num_random);

	run_test (random_name, random_edges, num_random);
    }

    return 0;
}
#endif

