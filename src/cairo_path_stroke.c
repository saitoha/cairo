/*
 * Copyright � 2002 USC, Information Sciences Institute
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

#include "cairoint.h"

typedef struct cairo_stroker {
    cairo_gstate_t *gstate;
    cairo_traps_t *traps;

    int have_prev;
    int have_first;
    int is_first;
    cairo_stroke_face_t prev;
    cairo_stroke_face_t first;
    int dash_index;
    int dash_on;
    double dash_remain;
} cairo_stroker_t;

/* private functions */
static void
_cairo_stroker_init (cairo_stroker_t *stroker, cairo_gstate_t *gstate, cairo_traps_t *traps);

static void
_cairo_stroker_fini (cairo_stroker_t *stroker);

static cairo_status_t
_cairo_stroker_add_edge (void *closure, cairo_point_t *p1, cairo_point_t *p2);

static cairo_status_t
_cairo_stroker_add_edge_dashed (void *closure, cairo_point_t *p1, cairo_point_t *p2);

static cairo_status_t
_cairo_stroker_add_spline (void *closure,
			   cairo_point_t *a, cairo_point_t *b,
			   cairo_point_t *c, cairo_point_t *d);

static cairo_status_t
_cairo_stroker_done_sub_path (void *closure, cairo_sub_path_done_t done);

static cairo_status_t
_cairo_stroker_done_path (void *closure);

static void
_translate_point (cairo_point_t *pt, cairo_point_t *offset);

static int
_cairo_stroker_face_clockwise (cairo_stroke_face_t *in, cairo_stroke_face_t *out);

static cairo_status_t
_cairo_stroker_join (cairo_stroker_t *stroker, cairo_stroke_face_t *in, cairo_stroke_face_t *out);

static void
_cairo_stroker_start_dash (cairo_stroker_t *stroker)
{
    cairo_gstate_t *gstate = stroker->gstate;
    double offset;
    int	on = 1;
    int	i = 0;

    offset = gstate->dash_offset;
    while (offset >= gstate->dash[i]) {
	offset -= gstate->dash[i];
	on = 1-on;
	if (++i == gstate->num_dashes)
	    i = 0;
    }
    stroker->dash_index = i;
    stroker->dash_on = on;
    stroker->dash_remain = gstate->dash[i] - offset;
}

static void
_cairo_stroker_step_dash (cairo_stroker_t *stroker, double step)
{
    cairo_gstate_t *gstate = stroker->gstate;
    stroker->dash_remain -= step;
    if (stroker->dash_remain <= 0) {
	stroker->dash_index++;
	if (stroker->dash_index == gstate->num_dashes)
	    stroker->dash_index = 0;
	stroker->dash_on = 1-stroker->dash_on;
	stroker->dash_remain = gstate->dash[stroker->dash_index];
    }
}

static void
_cairo_stroker_init (cairo_stroker_t *stroker, cairo_gstate_t *gstate, cairo_traps_t *traps)
{
    stroker->gstate = gstate;
    stroker->traps = traps;
    stroker->have_prev = 0;
    stroker->have_first = 0;
    stroker->is_first = 1;
    if (gstate->dash)
	_cairo_stroker_start_dash (stroker);
}

static void
_cairo_stroker_fini (cairo_stroker_t *stroker)
{
    /* nothing to do here */
}

static void
_translate_point (cairo_point_t *pt, cairo_point_t *offset)
{
    pt->x += offset->x;
    pt->y += offset->y;
}

static int
_cairo_stroker_face_clockwise (cairo_stroke_face_t *in, cairo_stroke_face_t *out)
{
    cairo_slope_t in_slope, out_slope;

    _cairo_slope_init (&in_slope, &in->pt, &in->cw);
    _cairo_slope_init (&out_slope, &out->pt, &out->cw);

    return _cairo_slope_clockwise (&in_slope, &out_slope);
}

static cairo_status_t
_cairo_stroker_join (cairo_stroker_t *stroker, cairo_stroke_face_t *in, cairo_stroke_face_t *out)
{
    cairo_status_t	status;
    cairo_gstate_t	*gstate = stroker->gstate;
    int		clockwise = _cairo_stroker_face_clockwise (out, in);
    cairo_point_t	*inpt, *outpt;

    if (in->cw.x == out->cw.x
	&& in->cw.y == out->cw.y
	&& in->ccw.x == out->ccw.x
	&& in->ccw.y == out->ccw.y) {
	return CAIRO_STATUS_SUCCESS;
    }

    if (clockwise) {
    	inpt = &in->ccw;
    	outpt = &out->ccw;
    } else {
    	inpt = &in->cw;
    	outpt = &out->cw;
    }

    switch (gstate->line_join) {
    case CAIRO_LINE_JOIN_ROUND: {
	int i;
	int start, step, stop;
	cairo_point_t tri[3], initial, final;
	cairo_pen_t *pen = &gstate->pen_regular;

	tri[0] = in->pt;
	if (clockwise) {
	    initial = in->ccw;
	    _cairo_pen_find_active_ccw_vertex_index (pen, &in->dev_vector, &start);
	    step = -1;
	    _cairo_pen_find_active_ccw_vertex_index (pen, &out->dev_vector, &stop);
	    final = out->ccw;
	} else {
	    initial = in->cw;
	    _cairo_pen_find_active_cw_vertex_index (pen, &in->dev_vector, &start);
	    step = +1;
	    _cairo_pen_find_active_cw_vertex_index (pen, &out->dev_vector, &stop);
	    final = out->cw;
	}

	i = start;
	tri[1] = initial;
	while (i != stop) {
	    tri[2] = in->pt;
	    _translate_point (&tri[2], &pen->vertex[i].pt);
	    cairo_traps_tessellate_triangle (stroker->traps, tri);
	    tri[1] = tri[2];
	    i += step;
	    if (i < 0)
		i = pen->num_vertices - 1;
	    if (i >= pen->num_vertices)
		i = 0;
	}

	tri[2] = final;

	return cairo_traps_tessellate_triangle (stroker->traps, tri);
    }
    case CAIRO_LINE_JOIN_MITER:
    default: {
	cairo_polygon_t	polygon;
	XDouble	c = (-in->usr_vector.x * out->usr_vector.x)+(-in->usr_vector.y * out->usr_vector.y);
	XDouble ml = gstate->miter_limit;

	_cairo_polygon_init (&polygon);

	if (2 <= ml * ml * (1 - c)) {
	    XDouble x1, y1, x2, y2;
	    XDouble mx, my;
	    XDouble dx1, dx2, dy1, dy2;
	    cairo_point_t	outer;

	    x1 = XFixedToDouble (inpt->x);
	    y1 = XFixedToDouble (inpt->y);
	    dx1 = in->usr_vector.x;
	    dy1 = in->usr_vector.y;
	    cairo_matrix_transform_distance (&gstate->ctm, &dx1, &dy1);
	    
	    x2 = XFixedToDouble (outpt->x);
	    y2 = XFixedToDouble (outpt->y);
	    dx2 = out->usr_vector.x;
	    dy2 = out->usr_vector.y;
	    cairo_matrix_transform_distance (&gstate->ctm, &dx2, &dy2);
	    
	    my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
		  (dx1 * dy2 - dx2 * dy1));
	    if (dy1)
		mx = (my - y1) * dx1 / dy1 + x1;
	    else
		mx = (my - y2) * dx2 / dy2 + x2;
	    
	    outer.x = XDoubleToFixed (mx);
	    outer.y = XDoubleToFixed (my);
	    _cairo_polygon_add_edge (&polygon, &in->pt, inpt);
	    _cairo_polygon_add_edge (&polygon, inpt, &outer);
	    _cairo_polygon_add_edge (&polygon, &outer, outpt);
	    _cairo_polygon_add_edge (&polygon, outpt, &in->pt);
	    status = cairo_traps_tessellate_polygon (stroker->traps,
						     &polygon,
						     CAIRO_FILL_RULE_WINDING);
	    _cairo_polygon_fini (&polygon);

	    return status;
	}
	/* fall through ... */
    }
    case CAIRO_LINE_JOIN_BEVEL: {
	cairo_point_t tri[3];
	tri[0] = in->pt;
	tri[1] = *inpt;
	tri[2] = *outpt;

	return cairo_traps_tessellate_triangle (stroker->traps, tri);
    }
    }
}

static cairo_status_t
_cairo_stroker_cap (cairo_stroker_t *stroker, cairo_stroke_face_t *f)
{
    cairo_status_t	    status;
    cairo_gstate_t	    *gstate = stroker->gstate;

    if (gstate->line_cap == CAIRO_LINE_CAP_BUTT)
	return CAIRO_STATUS_SUCCESS;
    
    switch (gstate->line_cap) {
    case CAIRO_LINE_CAP_ROUND: {
	int i;
	int start, stop;
	cairo_slope_t slope;
	cairo_point_t tri[3];
	cairo_pen_t *pen = &gstate->pen_regular;

	slope = f->dev_vector;
	_cairo_pen_find_active_cw_vertex_index (pen, &slope, &start);
	slope.dx = -slope.dx;
	slope.dy = -slope.dy;
	_cairo_pen_find_active_cw_vertex_index (pen, &slope, &stop);

	tri[0] = f->pt;
	tri[1] = f->cw;
	for (i=start; i != stop; i = (i+1) % pen->num_vertices) {
	    tri[2] = f->pt;
	    _translate_point (&tri[2], &pen->vertex[i].pt);
	    cairo_traps_tessellate_triangle (stroker->traps, tri);
	    tri[1] = tri[2];
	}
	tri[2] = f->ccw;

	return cairo_traps_tessellate_triangle (stroker->traps, tri);
    }
    case CAIRO_LINE_CAP_SQUARE: {
	double dx, dy;
	cairo_slope_t	fvector;
	cairo_point_t	occw, ocw;
	cairo_polygon_t	polygon;

	_cairo_polygon_init (&polygon);

	dx = f->usr_vector.x;
	dy = f->usr_vector.y;
	dx *= gstate->line_width / 2.0;
	dy *= gstate->line_width / 2.0;
	cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);
	fvector.dx = XDoubleToFixed (dx);
	fvector.dy = XDoubleToFixed (dy);
	occw.x = f->ccw.x + fvector.dx;
	occw.y = f->ccw.y + fvector.dy;
	ocw.x = f->cw.x + fvector.dx;
	ocw.y = f->cw.y + fvector.dy;

	_cairo_polygon_add_edge (&polygon, &f->cw, &ocw);
	_cairo_polygon_add_edge (&polygon, &ocw, &occw);
	_cairo_polygon_add_edge (&polygon, &occw, &f->ccw);
	_cairo_polygon_add_edge (&polygon, &f->ccw, &f->cw);

	status = cairo_traps_tessellate_polygon (stroker->traps, &polygon, CAIRO_FILL_RULE_WINDING);
	_cairo_polygon_fini (&polygon);

	return status;
    }
    case CAIRO_LINE_CAP_BUTT:
    default:
	return CAIRO_STATUS_SUCCESS;
    }
}

static void
_compute_face (cairo_point_t *pt, cairo_slope_t *slope, cairo_gstate_t *gstate, cairo_stroke_face_t *face)
{
    double mag, tmp;
    double dx, dy;
    XPointDouble usr_vector;
    cairo_point_t offset_ccw, offset_cw;

    dx = XFixedToDouble (slope->dx);
    dy = XFixedToDouble (slope->dy);

    cairo_matrix_transform_distance (&gstate->ctm_inverse, &dx, &dy);

    mag = sqrt (dx * dx + dy * dy);
    if (mag == 0) {
	/* XXX: Can't compute other face points. Do we want a tag in the face for this case? */
	return;
    }

    dx /= mag;
    dy /= mag;

    usr_vector.x = dx;
    usr_vector.y = dy;

    tmp = dx;
    dx = - dy * (gstate->line_width / 2.0);
    dy = tmp * (gstate->line_width / 2.0);

    cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);

    offset_ccw.x = XDoubleToFixed (dx);
    offset_ccw.y = XDoubleToFixed (dy);
    offset_cw.x = -offset_ccw.x;
    offset_cw.y = -offset_ccw.y;

    face->ccw = *pt;
    _translate_point (&face->ccw, &offset_ccw);

    face->pt = *pt;

    face->cw = *pt;
    _translate_point (&face->cw, &offset_cw);

    face->usr_vector.x = usr_vector.x;
    face->usr_vector.y = usr_vector.y;

    face->dev_vector = *slope;
}

static cairo_status_t
_cairo_stroker_add_sub_edge (cairo_stroker_t *stroker, cairo_point_t *p1, cairo_point_t *p2,
			     cairo_stroke_face_t *start, cairo_stroke_face_t *end)
{
    cairo_gstate_t *gstate = stroker->gstate;
    cairo_point_t quad[4];
    cairo_slope_t slope;

    if (p1->x == p2->x && p1->y == p2->y) {
	/* XXX: Need to rethink how this case should be handled, (both
           here and in _compute_face). The key behavior is that
           degenerate paths should draw as much as possible. */
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_slope_init (&slope, p1, p2);
    _compute_face (p1, &slope, gstate, start);

    /* XXX: This could be optimized slightly by not calling
       _compute_face again but rather  translating the relevant
       fields from start. */
    _compute_face (p2, &slope, gstate, end);

    quad[0] = start->cw;
    quad[1] = start->ccw;
    quad[2] = end->ccw;
    quad[3] = end->cw;

    return cairo_traps_tessellate_rectangle (stroker->traps, quad);
}

static cairo_status_t
_cairo_stroker_add_edge (void *closure, cairo_point_t *p1, cairo_point_t *p2)
{
    cairo_status_t status;
    cairo_stroker_t *stroker = closure;
    cairo_stroke_face_t start, end;

    if (p1->x == p2->x && p1->y == p2->y) {
	/* XXX: Need to rethink how this case should be handled, (both
           here and in cairo_stroker_add_sub_edge and in _compute_face). The
           key behavior is that degenerate paths should draw as much
           as possible. */
	return CAIRO_STATUS_SUCCESS;
    }
    
    status = _cairo_stroker_add_sub_edge (stroker, p1, p2, &start, &end);
    if (status)
	return status;

    if (stroker->have_prev) {
	status = _cairo_stroker_join (stroker, &stroker->prev, &start);
	if (status)
	    return status;
    } else {
	stroker->have_prev = 1;
	if (stroker->is_first) {
	    stroker->have_first = 1;
	    stroker->first = start;
	}
    }
    stroker->prev = end;
    stroker->is_first = 0;

    return CAIRO_STATUS_SUCCESS;
}

/*
 * Dashed lines.  Cap each dash end, join around turns when on
 */
static cairo_status_t
_cairo_stroker_add_edge_dashed (void *closure, cairo_point_t *p1, cairo_point_t *p2)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_stroker_t *stroker = closure;
    cairo_gstate_t *gstate = stroker->gstate;
    double mag, remain, tmp;
    double dx, dy;
    double dx2, dy2;
    cairo_point_t fd1, fd2;
    int first = 1;
    cairo_stroke_face_t sub_start, sub_end;
    
    dx = XFixedToDouble (p2->x - p1->x);
    dy = XFixedToDouble (p2->y - p1->y);

    cairo_matrix_transform_distance (&gstate->ctm_inverse, &dx, &dy);

    mag = sqrt (dx *dx + dy * dy);
    remain = mag;
    fd1 = *p1;
    while (remain) {
	tmp = stroker->dash_remain;
	if (tmp > remain)
	    tmp = remain;
	remain -= tmp;
        dx2 = dx * (mag - remain)/mag;
	dy2 = dy * (mag - remain)/mag;
	cairo_matrix_transform_distance (&gstate->ctm, &dx2, &dy2);
	fd2.x = XDoubleToFixed (dx2);
	fd2.y = XDoubleToFixed (dy2);
	fd2.x += p1->x;
	fd2.y += p1->y;
	/*
	 * XXX simplify this case analysis
	 */
	if (stroker->dash_on) {
	    status = _cairo_stroker_add_sub_edge (stroker, &fd1, &fd2, &sub_start, &sub_end);
	    if (status)
		return status;
	    if (!first) {
		/*
		 * Not first dash in this segment, cap start
		 */
		status = _cairo_stroker_cap (stroker, &sub_start);
		if (status)
		    return status;
	    } else {
		/*
		 * First in this segment, join to any prev, else
		 * if at start of sub-path, mark position, else
		 * cap
		 */
		if (stroker->have_prev) {
		    status = _cairo_stroker_join (stroker, &stroker->prev, &sub_start);
		    if (status)
			return status;
		} else {
		    if (stroker->is_first) {
			stroker->have_first = 1;
			stroker->first = sub_start;
		    } else {
			status = _cairo_stroker_cap (stroker, &sub_start);
			if (status)
			    return status;
		    }
		}
	    }
	    if (remain) {
		/*
		 * Cap if not at end of segment
		 */
		status = _cairo_stroker_cap (stroker, &sub_end);
		if (status)
		    return status;
	    } else {
		/*
		 * Mark previous line face and fix up next time
		 * through
		 */
		stroker->prev = sub_end;
		stroker->have_prev = 1;
	    }
	} else {
	    /*
	     * If starting with off dash, check previous face
	     * and cap if necessary
	     */
	    if (first) {
		if (stroker->have_prev) {
		    status = _cairo_stroker_cap (stroker, &stroker->prev);
		    if (status)
			return status;
		}
	    }
	    if (!remain)
		stroker->have_prev = 0;
	}
	_cairo_stroker_step_dash (stroker, tmp);
	fd1 = fd2;
	first = 0;
    }
    stroker->is_first = 0;
    return status;
}

static cairo_status_t
_cairo_stroker_add_spline (void *closure,
			   cairo_point_t *a, cairo_point_t *b,
			   cairo_point_t *c, cairo_point_t *d)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_stroker_t *stroker = closure;
    cairo_gstate_t *gstate = stroker->gstate;
    cairo_spline_t spline;
    cairo_pen_t pen;
    cairo_stroke_face_t start, end;
    cairo_point_t extra_points[4];

    status = _cairo_spline_init (&spline, a, b, c, d);
    if (status == cairo_int_status_degenerate)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_pen_init_copy (&pen, &gstate->pen_regular);
    if (status)
	goto CLEANUP_SPLINE;

    _compute_face (a, &spline.initial_slope, gstate, &start);
    _compute_face (d, &spline.final_slope, gstate, &end);

    if (stroker->have_prev) {
	status = _cairo_stroker_join (stroker, &stroker->prev, &start);
	if (status)
	    return status;
    } else {
	stroker->have_prev = 1;
	if (stroker->is_first) {
	    stroker->have_first = 1;
	    stroker->first = start;
	}
    }
    stroker->prev = end;
    stroker->is_first = 0;
    
    extra_points[0] = start.cw;
    extra_points[0].x -= start.pt.x;
    extra_points[0].y -= start.pt.y;
    extra_points[1] = start.ccw;
    extra_points[1].x -= start.pt.x;
    extra_points[1].y -= start.pt.y;
    extra_points[2] = end.cw;
    extra_points[2].x -= end.pt.x;
    extra_points[2].y -= end.pt.y;
    extra_points[3] = end.ccw;
    extra_points[3].x -= end.pt.x;
    extra_points[3].y -= end.pt.y;
    
    status = _cairo_pen_add_points (&pen, extra_points, 4);
    if (status)
	goto CLEANUP_PEN;

    status = _cairo_pen_stroke_spline (&pen, &spline, gstate->tolerance, stroker->traps);
    if (status)
	goto CLEANUP_PEN;

  CLEANUP_PEN:
    _cairo_pen_fini (&pen);
  CLEANUP_SPLINE:
    _cairo_spline_fini (&spline);

    return status;
}

static cairo_status_t
_cairo_stroker_done_sub_path (void *closure, cairo_sub_path_done_t done)
{
    cairo_status_t status;
    cairo_stroker_t *stroker = closure;

    switch (done) {
    case cairo_sub_path_done_join:
	if (stroker->have_first && stroker->have_prev) {
	    status = _cairo_stroker_join (stroker, &stroker->prev, &stroker->first);
	    if (status)
		return status;
	    break;
	}
	/* fall through... */
    case cairo_sub_path_done_cap:
	if (stroker->have_first) {
	    cairo_point_t t;
	    /* The initial cap needs an outward facing vector. Reverse everything */
	    stroker->first.usr_vector.x = -stroker->first.usr_vector.x;
	    stroker->first.usr_vector.y = -stroker->first.usr_vector.y;
	    stroker->first.dev_vector.dx = -stroker->first.dev_vector.dx;
	    stroker->first.dev_vector.dy = -stroker->first.dev_vector.dy;
	    t = stroker->first.cw;
	    stroker->first.cw = stroker->first.ccw;
	    stroker->first.ccw = t;
	    status = _cairo_stroker_cap (stroker, &stroker->first);
	    if (status)
		return status;
	}
	if (stroker->have_prev) {
	    status = _cairo_stroker_cap (stroker, &stroker->prev);
	    if (status)
		return status;
	}
	break;
    }

    stroker->have_prev = 0;
    stroker->have_first = 0;
    stroker->is_first = 1;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_stroker_done_path (void *closure)
{
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_stroke_to_traps (cairo_path_t *path, cairo_gstate_t *gstate, cairo_traps_t *traps)
{
    static const cairo_path_callbacks_t stroker_solid_cb = {
	_cairo_stroker_add_edge,
	_cairo_stroker_add_spline,
	_cairo_stroker_done_sub_path,
	_cairo_stroker_done_path
    };
    static const cairo_path_callbacks_t stroker_dashed_cb = {
	_cairo_stroker_add_edge_dashed,
	_cairo_stroker_add_spline,
	_cairo_stroker_done_sub_path,
	_cairo_stroker_done_path
    };
    const cairo_path_callbacks_t *callbacks = gstate->dash ? &stroker_dashed_cb : &stroker_solid_cb;

    cairo_status_t status;
    cairo_stroker_t stroker;

    _cairo_stroker_init (&stroker, gstate, traps);

    status = _cairo_path_interpret (path,
				    cairo_path_direction_forward,
				    callbacks, &stroker);
    if (status) {
	_cairo_stroker_fini (&stroker);
	return status;
    }

    _cairo_stroker_fini (&stroker);

    return CAIRO_STATUS_SUCCESS;
}
