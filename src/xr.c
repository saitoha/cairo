/*
 * $XFree86: $
 *
 * Copyright � 2002 Carl D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Carl
 * D. Worth not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Carl D. Worth makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CARL D. WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CARL D. WORTH BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "xrint.h"

#define _XR_CURRENT_GSTATE(xrs) (xrs->stack)

#define XR_TOLERANCE_MINIMUM	0.0002 /* We're limited by 16 bits of sub-pixel precision */

static void
_XrClipValue(double *value, double min, double max);

XrState *
XrCreate(void)
{
    return _XrStateCreate();
}

void
XrDestroy(XrState *xrs)
{
    _XrStateDestroy(xrs);
}

void
XrSave(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrStatePush(xrs);
}

void
XrRestore(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrStatePop(xrs);
}

/* XXX: I want to rethink this API
void
XrPushGroup(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrStatePush(xrs);
    if (xrs->status)
	return;

    xrs->status = _XrGStateBeginGroup(_XR_CURRENT_GSTATE(xrs));
}

void
XrPopGroup(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateEndGroup(_XR_CURRENT_GSTATE(xrs));
    if (xrs->status)
	return;

    xrs->status = _XrStatePop(xrs);
}
*/

void
XrSetTargetSurface (XrState *xrs, XrSurface *surface)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetTargetSurface (_XR_CURRENT_GSTATE (xrs), surface);
}

XrSurface *
XrGetTargetSurface (XrState *xrs)
{
    return _XrGStateGetTargetSurface (_XR_CURRENT_GSTATE (xrs));
}

void
XrSetTargetDrawable (XrState	*xrs,
		     Display	*dpy,
		     Drawable	drawable)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetTargetDrawable (_XR_CURRENT_GSTATE (xrs),
					      dpy, drawable,
					      DefaultVisual (dpy, DefaultScreen (dpy)),
					      0);
}

void
XrSetOperator(XrState *xrs, XrOperator op)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetOperator(_XR_CURRENT_GSTATE(xrs), op);
}

XrOperator
XrGetOperator(XrState *xrs)
{
    return _XrGStateGetOperator(_XR_CURRENT_GSTATE(xrs));
}

void
XrSetRGBColor(XrState *xrs, double red, double green, double blue)
{
    if (xrs->status)
	return;

    _XrClipValue(&red, 0.0, 1.0);
    _XrClipValue(&green, 0.0, 1.0);
    _XrClipValue(&blue, 0.0, 1.0);

    xrs->status = _XrGStateSetRGBColor(_XR_CURRENT_GSTATE(xrs), red, green, blue);
}

void
XrSetTolerance(XrState *xrs, double tolerance)
{
    if (xrs->status)
	return;

    _XrClipValue(&tolerance, XR_TOLERANCE_MINIMUM, tolerance);

    xrs->status = _XrGStateSetTolerance(_XR_CURRENT_GSTATE(xrs), tolerance);
}

double
XrGetTolerance(XrState *xrs)
{
    return _XrGStateGetTolerance(_XR_CURRENT_GSTATE(xrs));
}

void
XrSetAlpha(XrState *xrs, double alpha)
{
    if (xrs->status)
	return;

    _XrClipValue(&alpha, 0.0, 1.0);

    xrs->status = _XrGStateSetAlpha(_XR_CURRENT_GSTATE(xrs), alpha);
}

void
XrSetFillRule(XrState *xrs, XrFillRule fill_rule)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetFillRule(_XR_CURRENT_GSTATE(xrs), fill_rule);
}

void
XrSetLineWidth(XrState *xrs, double width)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetLineWidth(_XR_CURRENT_GSTATE(xrs), width);
}

double
XrGetLineWidth(XrState *xrs)
{
    return _XrGStateGetLineWidth(_XR_CURRENT_GSTATE(xrs));
}

void
XrSetLineCap(XrState *xrs, XrLineCap line_cap)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetLineCap(_XR_CURRENT_GSTATE(xrs), line_cap);
}

XrLineCap
XrGetLineCap(XrState *xrs)
{
    return _XrGStateGetLineCap(_XR_CURRENT_GSTATE(xrs));
}

void
XrSetLineJoin(XrState *xrs, XrLineJoin line_join)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetLineJoin(_XR_CURRENT_GSTATE(xrs), line_join);
}

XrLineJoin
XrGetLineJoin(XrState *xrs)
{
    return _XrGStateGetLineJoin(_XR_CURRENT_GSTATE(xrs));
}

void
XrSetDash(XrState *xrs, double *dashes, int ndash, double offset)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetDash(_XR_CURRENT_GSTATE(xrs), dashes, ndash, offset);
}

void
XrSetMiterLimit(XrState *xrs, double limit)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetMiterLimit(_XR_CURRENT_GSTATE(xrs), limit);
}

double
XrGetMiterLimit(XrState *xrs)
{
    return _XrGStateGetMiterLimit(_XR_CURRENT_GSTATE(xrs));
}

void
XrTranslate(XrState *xrs, double tx, double ty)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateTranslate(_XR_CURRENT_GSTATE(xrs), tx, ty);
}

void
XrScale(XrState *xrs, double sx, double sy)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateScale(_XR_CURRENT_GSTATE(xrs), sx, sy);
}

void
XrRotate(XrState *xrs, double angle)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateRotate(_XR_CURRENT_GSTATE(xrs), angle);
}

void
XrConcatMatrix(XrState *xrs,
	       double a, double b,
	       double c, double d,
	       double tx, double ty)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateConcatMatrix(_XR_CURRENT_GSTATE(xrs), a, b, c, d, tx, ty);
}

void
XrSetMatrix(XrState *xrs,
	       double a, double b,
	       double c, double d,
	       double tx, double ty)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSetMatrix(_XR_CURRENT_GSTATE(xrs), a, b, c, d, tx, ty);
}

void
XrDefaultMatrix(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateIdentityMatrix(_XR_CURRENT_GSTATE(xrs));
}

void
XrIdentityMatrix(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateIdentityMatrix(_XR_CURRENT_GSTATE(xrs));
}

void
XrNewPath(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateNewPath(_XR_CURRENT_GSTATE(xrs));
}

void
XrMoveTo(XrState *xrs, double x, double y)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateMoveTo(_XR_CURRENT_GSTATE(xrs), x, y);
}

void
XrLineTo(XrState *xrs, double x, double y)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateLineTo(_XR_CURRENT_GSTATE(xrs), x, y);
}

void
XrCurveTo(XrState *xrs,
	  double x1, double y1,
	  double x2, double y2,
	  double x3, double y3)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateCurveTo(_XR_CURRENT_GSTATE(xrs),
				   x1, y1,
				   x2, y2,
				   x3, y3);
}

void
XrRelMoveTo(XrState *xrs, double dx, double dy)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateRelMoveTo(_XR_CURRENT_GSTATE(xrs), dx, dy);
}

void
XrRelLineTo(XrState *xrs, double dx, double dy)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateRelLineTo(_XR_CURRENT_GSTATE(xrs), dx, dy);
}

void
XrRelCurveTo(XrState *xrs,
	     double dx1, double dy1,
	     double dx2, double dy2,
	     double dx3, double dy3)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateRelCurveTo(_XR_CURRENT_GSTATE(xrs),
				      dx1, dy1,
				      dx2, dy2,
				      dx3, dy3);
}

void
XrClosePath(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateClosePath(_XR_CURRENT_GSTATE(xrs));
}

void
XrGetCurrentPoint(XrState *xrs, double *x, double *y)
{
    /* XXX: Should we do anything with the return value in the error case? */
    if (xrs->status)
	return;

    xrs->status = _XrGStateGetCurrentPoint(_XR_CURRENT_GSTATE(xrs), x, y);
}

void
XrStroke(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateStroke(_XR_CURRENT_GSTATE(xrs));
}

void
XrFill(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateFill(_XR_CURRENT_GSTATE(xrs));
}

void
XrClip(XrState *xrs)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateClip(_XR_CURRENT_GSTATE(xrs));
}

void
XrSelectFont(XrState *xrs, const char *key)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateSelectFont(_XR_CURRENT_GSTATE(xrs), key);
}

void
XrScaleFont(XrState *xrs, double scale)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateScaleFont(_XR_CURRENT_GSTATE(xrs), scale);
}

void
XrTransformFont(XrState *xrs,
		double a, double b,
		double c, double d)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateTransformFont(_XR_CURRENT_GSTATE(xrs),
					 a, b, c, d);
}

void
XrTextExtents(XrState *xrs,
	      const unsigned char *utf8,
	      double *x, double *y,
	      double *width, double *height,
	      double *dx, double *dy)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateTextExtents(_XR_CURRENT_GSTATE(xrs), utf8,
				       x, y, width, height, dx, dy);
}

void
XrShowText(XrState *xrs, const unsigned char *utf8)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateShowText(_XR_CURRENT_GSTATE(xrs), utf8);
}

void
XrShowImage(XrState		*xrs,
	    char		*data,
	    XrFormat		format,
	    unsigned int	width,
	    unsigned int	height,
	    unsigned int	stride)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateShowImage(_XR_CURRENT_GSTATE(xrs),
				     data, format,
				     width, height, stride);
}

void
XrShowImageTransform(XrState		*xrs,
		     char		*data,
		     XrFormat		format,
		     unsigned int	width,
		     unsigned int	height,
		     unsigned int	stride,
		     double a, double b,
		     double c, double d,
		     double tx, double ty)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateShowImageTransform(_XR_CURRENT_GSTATE(xrs),
					      data, format,
					      width, height, stride,
					      a, b,
					      c, d,
					      tx, ty);
}

void
XrShowSurface (XrState		*xrs,
	       XrSurface	*surface,
	       int		x,
	       int		y,
	       int		width,
	       int		height)
{
    if (xrs->status)
	return;

    xrs->status = _XrGStateShowSurface (_XR_CURRENT_GSTATE (xrs),
					surface,
					x, y,
					width, height);
}

XrStatus
XrGetStatus(XrState *xrs)
{
    return xrs->status;
}

const char *
XrGetStatusString(XrState *xrs)
{
    switch (xrs->status) {
    case XrStatusSuccess:
	return "success";
    case XrStatusNoMemory:
	return "out of memory";
    case XrStatusInvalidRestore:
	return "XrRestore without matching XrSave";
    case XrStatusInvalidPopGroup:
	return "XrPopGroup without matching XrPushGroup";
    case XrStatusNoCurrentPoint:
	return "no current point defined";
    case XrStatusInvalidMatrix:
	return "invalid matrix (not invertible)";
    }

    return "";
}

static void
_XrClipValue(double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}
