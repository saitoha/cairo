/*
 * $XFree86: $
 *
 * Copyright � 2002 University of Southern California
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of University
 * of Southern California not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  University of Southern California makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl Worth, USC, Information Sciences Institute */

#include "xrint.h"

XrState *
XrCreate(Display *dpy)
{
    return XrStateCreate(dpy);
}

void
XrDestroy(XrState *xrs)
{
    XrStateDestroy(xrs);
}

void
XrSave(XrState *xrs)
{
    XrStatePush(xrs);
}

void
XrRestore(XrState *xrs)
{
    XrStatePop(xrs);
}

void
XrSetDrawable(XrState *xrs, Drawable drawable)
{
    XrGStateSetDrawable(CURRENT_GSTATE(xrs), drawable);
}

void
XrSetVisual(XrState *xrs, Visual *visual)
{
    XrGStateSetVisual(CURRENT_GSTATE(xrs), visual);
}

void
XrSetFormat(XrState *xrs, XrFormat format)
{
    XrGStateSetFormat(CURRENT_GSTATE(xrs), format);
}

void
XrSetOperator(XrState *xrs, XrOperator operator)
{
    XrGStateSetOperator(CURRENT_GSTATE(xrs), operator);
}

void
XrSetRGBColor(XrState *xrs, double red, double green, double blue)
{
    XrGStateSetRGBColor(CURRENT_GSTATE(xrs), red, green, blue);
}

void
XrSetAlpha(XrState *xrs, double alpha)
{
    XrGStateSetAlpha(CURRENT_GSTATE(xrs), alpha);
}

void
XrSetLineWidth(XrState *xrs, double width)
{
    XrGStateSetLineWidth(CURRENT_GSTATE(xrs), width);
}

void
XrSetLineCap(XrState *xrs, XrLineCap line_cap)
{
    XrGStateSetLineCap(CURRENT_GSTATE(xrs), line_cap);
}

void
XrSetLineJoin(XrState *xrs, XrLineJoin line_join)
{
    XrGStateSetLineJoin(CURRENT_GSTATE(xrs), line_join);
}

void
XrSetMiterLimit(XrState *xrs, double limit)
{
    XrGStateSetMiterLimit(CURRENT_GSTATE(xrs), limit);
}

void
XrTranslate(XrState *xrs, double tx, double ty)
{
    XrGStateTranslate(CURRENT_GSTATE(xrs), tx, ty);
}

void
XrScale(XrState *xrs, double sx, double sy)
{
    XrGStateScale(CURRENT_GSTATE(xrs), sx, sy);
}

void
XrRotate(XrState *xrs, double angle)
{
    XrGStateRotate(CURRENT_GSTATE(xrs), angle);
}

void
XrNewPath(XrState *xrs)
{
    XrGStateNewPath(CURRENT_GSTATE(xrs));
}

void
XrMoveTo(XrState *xrs, double x, double y)
{
    XrGStateMoveTo(CURRENT_GSTATE(xrs), x, y);
}

void
XrLineTo(XrState *xrs, double x, double y)
{
    XrGStateLineTo(CURRENT_GSTATE(xrs), x, y);
}

void
XrRelMoveTo(XrState *xrs, double x, double y)
{
    XrGStateRelMoveTo(CURRENT_GSTATE(xrs), x, y);
}

void
XrRelLineTo(XrState *xrs, double x, double y)
{
    XrGStateRelLineTo(CURRENT_GSTATE(xrs), x, y);
}

void
XrClosePath(XrState *xrs)
{
    XrGStateClosePath(CURRENT_GSTATE(xrs));
}

void
XrStroke(XrState *xrs)
{
    XrGStateStroke(CURRENT_GSTATE(xrs));
}

void
XrFill(XrState *xrs)
{
    XrGStateFill(CURRENT_GSTATE(xrs));
}

