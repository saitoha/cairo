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

#ifndef _XR_H_
#define _XR_H_

#include <X11/Xc/Xc.h>

typedef struct _XrState XrState;

/* Functions for manipulating state objects */
XrState *
XrCreate(Display *dpy);

void
XrDestroy(XrState *xrs);

void
XrSave(XrState *xrs);

void
XrRestore(XrState *xrs);

void
XrPushGroup(XrState *xrs);

void
XrPopGroup(XrState *xrs);

/* Modify state */
void
XrSetDrawable(XrState *xrs, Drawable drawable);

void
XrSetVisual(XrState *xrs, Visual *visual);

typedef enum _XrFormat {
    XrFormatARGB32 = PictStandardARGB32,
    XrFormatRGB32 = PictStandardRGB24,
    XrFormatA8 = PictStandardA8,
    XrFormatA1 = PictStandardA1
} XrFormat;
 
void
XrSetFormat(XrState *xrs, XrFormat format);

typedef enum _XrOperator { 
    XrOperatorClear = PictOpClear,
    XrOperatorSrc = PictOpSrc,
    XrOperatorDst = PictOpDst,
    XrOperatorOver = PictOpOver,
    XrOperatorOverReverse = PictOpOverReverse,
    XrOperatorIn = PictOpIn,
    XrOperatorInReverse = PictOpInReverse,
    XrOperatorOut = PictOpOut,
    XrOperatorOutReverse = PictOpOutReverse,
    XrOperatorAtop = PictOpAtop,
    XrOperatorAtopReverse = PictOpAtopReverse,
    XrOperatorXor = PictOpXor,
    XrOperatorAdd = PictOpAdd,
    XrOperatorSaturate = PictOpSaturate,

    XrOperatorDisjointClear = PictOpDisjointClear,
    XrOperatorDisjointSrc = PictOpDisjointSrc,
    XrOperatorDisjointDst = PictOpDisjointDst,
    XrOperatorDisjointOver = PictOpDisjointOver,
    XrOperatorDisjointOverReverse = PictOpDisjointOverReverse,
    XrOperatorDisjointIn = PictOpDisjointIn,
    XrOperatorDisjointInReverse = PictOpDisjointInReverse,
    XrOperatorDisjointOut = PictOpDisjointOut,
    XrOperatorDisjointOutReverse = PictOpDisjointOutReverse,
    XrOperatorDisjointAtop = PictOpDisjointAtop,
    XrOperatorDisjointAtopReverse = PictOpDisjointAtopReverse,
    XrOperatorDisjointXor = PictOpDisjointXor,

    XrOperatorConjointClear = PictOpConjointClear,
    XrOperatorConjointSrc = PictOpConjointSrc,
    XrOperatorConjointDst = PictOpConjointDst,
    XrOperatorConjointOver = PictOpConjointOver,
    XrOperatorConjointOverReverse = PictOpConjointOverReverse,
    XrOperatorConjointIn = PictOpConjointIn,
    XrOperatorConjointInReverse = PictOpConjointInReverse,
    XrOperatorConjointOut = PictOpConjointOut,
    XrOperatorConjointOutReverse = PictOpConjointOutReverse,
    XrOperatorConjointAtop = PictOpConjointAtop,
    XrOperatorConjointAtopReverse = PictOpConjointAtopReverse,
    XrOperatorConjointXor = PictOpConjointXor
} XrOperator;

void
XrSetOperator(XrState *xrs, XrOperator operator);

void
XrSetRGBColor(XrState *xrs, double red, double green, double blue);

void
XrSetTolerance(XrState *xrs, double tolerance);

void
XrSetAlpha(XrState *xrs, double alpha);

typedef enum _XrFillRule { XrFillRuleWinding, XrFillRuleEvenOdd } XrFillRule;

void
XrSetFillRule(XrState *xrs, XrFillRule fill_rule);

void
XrSetLineWidth(XrState *xrs, double width);

typedef enum _XrLineCap { XrLineCapButt, XrLineCapRound, XrLineCapSquare } XrLineCap;

void
XrSetLineCap(XrState *xrs, XrLineCap line_cap);

typedef enum _XrLineJoin { XrLineJoinMiter, XrLineJoinRound, XrLineJoinBevel } XrLineJoin;

void
XrSetLineJoin(XrState *xrs, XrLineJoin line_join);

void
XrSetDash(XrState *xrs, double *dashes, int ndash, double offset);

void
XrSetMiterLimit(XrState *xrs, double limit);

void
XrTranslate(XrState *xrs, double tx, double ty);

void
XrScale(XrState *xrs, double sx, double sy);

void
XrRotate(XrState *xrs, double angle);

void
XrConcatMatrix(XrState *xrs,
	       double a, double b,
	       double c, double d,
	       double tx, double ty);

void
XrSetMatrix(XrState *xrs,
	       double a, double b,
	       double c, double d,
	       double tx, double ty);

/* XXX: Postscript has both a defaultmatrix and an identmatrix. But
   there, they do different things. Here, where they perform the same
   function, we should probably only have one name to avoid
   confusion. Any votes? */
void
XrDefaultMatrix(XrState *xrs);

void
XrIdentityMatrix(XrState *xrs);

/* Path creation functions */
void
XrNewPath(XrState *xrs);

void
XrMoveTo(XrState *xrs, double x, double y);

void
XrLineTo(XrState *xrs, double x, double y);

void
XrCurveTo(XrState *xrs,
	  double x1, double y1,
	  double x2, double y2,
	  double x3, double y3);

void
XrRelMoveTo(XrState *xrs, double dx, double dy);

void
XrRelLineTo(XrState *xrs, double dx, double dy);

void
XrRelCurveTo(XrState *xrs,
	     double dx1, double dy1,
	     double dx2, double dy2,
	     double dx3, double dy3);

void
XrClosePath(XrState *xrs);

/* Painting functions */
void
XrStroke(XrState *xrs);

void
XrFill(XrState *xrs);

/* Font/Text functions */
void
XrSelectFont(XrState *xrs, const char *key);

void
XrScaleFont(XrState *xrs, double scale);

void
XrTransformFont(XrState *xrs,
		double a, double b,
		double c, double d);

void
XrTextExtents(XrState *xrs,
	      const unsigned char *utf8,
	      double *x, double *y,
	      double *width, double *height,
	      double *dx, double *dy);

void
XrShowText(XrState *xrs, const unsigned char *utf8);

/* Image functions */

void
XrShowImage(XrState		*xrs,
	    char		*data,
	    XrFormat		format,
	    unsigned int	width,
	    unsigned int	height,
	    unsigned int	stride);

void
XrShowImageTransform(XrState		*xrs,
		     char		*data,
		     XrFormat		format,
		     unsigned int	width,
		     unsigned int	height,
		     unsigned int	stride,
		     double a, double b,
		     double c, double d,
		     double tx, double ty);

/* Error status queries */

typedef enum _XrStatus {
    XrStatusSuccess = 0,
    XrStatusNoMemory,
    XrStatusInvalidRestore,
    XrStatusInvalidPopGroup,
    XrStatusNoCurrentPoint,
    XrStatusInvalidMatrix
} XrStatus;

XrStatus
XrGetStatus(XrState *xrs);

const char *
XrGetStatusString(XrState *xrs);

#endif

