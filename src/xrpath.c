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

#include <stdlib.h>
#include "xrint.h"

/* private functions */
static XrStatus
_XrPathAdd(XrPath *path, XrPathOp op, XPointFixed *pts, int num_pts);

static void
_XrPathAddOpBuf(XrPath *path, XrPathOpBuf *op);

static XrStatus
_XrPathNewOpBuf(XrPath *path);

static void
_XrPathAddArgBuf(XrPath *path, XrPathArgBuf *arg);

static XrStatus
_XrPathNewArgBuf(XrPath *path);

static XrPathOpBuf *
_XrPathOpBufCreate(void);

static void
_XrPathOpBufDestroy(XrPathOpBuf *buf);

static void
_XrPathOpBufAdd(XrPathOpBuf *op_buf, XrPathOp op);

static XrPathArgBuf *
_XrPathArgBufCreate(void);

static void
_XrPathArgBufDestroy(XrPathArgBuf *buf);

static void
_XrPathArgBufAdd(XrPathArgBuf *arg, XPointFixed *pts, int num_pts);

void
_XrPathInit(XrPath *path)
{
    path->op_head = NULL;
    path->op_tail = NULL;

    path->arg_head = NULL;
    path->arg_tail = NULL;
}

XrStatus
_XrPathInitCopy(XrPath *path, XrPath *other)
{
    XrPathOpBuf *op, *other_op;
    XrPathArgBuf *arg, *other_arg;

    _XrPathInit(path);

    for (other_op = other->op_head; other_op; other_op = other_op->next) {
	op = _XrPathOpBufCreate();
	if (op == NULL) {
	    return XrStatusNoMemory;
	}
	*op = *other_op;
	_XrPathAddOpBuf(path, op);
    }

    for (other_arg = other->arg_head; other_arg; other_arg = other_arg->next) {
	arg = _XrPathArgBufCreate();
	if (arg == NULL) {
	    return XrStatusNoMemory;
	}
	*arg = *other_arg;
	_XrPathAddArgBuf(path, arg);
    }

    return XrStatusSuccess;
}

void
_XrPathDeinit(XrPath *path)
{
    XrPathOpBuf *op;
    XrPathArgBuf *arg;

    while (path->op_head) {
	op = path->op_head;
	path->op_head = op->next;
	_XrPathOpBufDestroy(op);
    }
    path->op_tail = NULL;

    while (path->arg_head) {
	arg = path->arg_head;
	path->arg_head = arg->next;
	_XrPathArgBufDestroy(arg);
    }
    path->arg_tail = NULL;
}

XrStatus
_XrPathMoveTo(XrPath *path, double x, double y)
{
    XPointFixed pt;

    pt.x = XDoubleToFixed(x);
    pt.y = XDoubleToFixed(y);

    return _XrPathAdd(path, XrPathOpMoveTo, &pt, 1);
}

XrStatus
_XrPathLineTo(XrPath *path, double x, double y)
{
    XPointFixed pt;

    pt.x = XDoubleToFixed(x);
    pt.y = XDoubleToFixed(y);

    return _XrPathAdd(path, XrPathOpLineTo, &pt, 1);
}

XrStatus
_XrPathCurveTo(XrPath *path,
	       double x1, double y1,
	       double x2, double y2,
	       double x3, double y3)
{
    XPointFixed pt[3];

    pt[0].x = XDoubleToFixed(x1);
    pt[0].y = XDoubleToFixed(y1);

    pt[1].x = XDoubleToFixed(x2);
    pt[1].y = XDoubleToFixed(y2);

    pt[2].x = XDoubleToFixed(x3);
    pt[2].y = XDoubleToFixed(y3);

    return _XrPathAdd(path, XrPathOpCurveTo, pt, 3);
}

XrStatus
_XrPathClosePath(XrPath *path)
{
    return _XrPathAdd(path, XrPathOpClosePath, NULL, 0);
}

static XrStatus
_XrPathAdd(XrPath *path, XrPathOp op, XPointFixed *pts, int num_pts)
{
    XrStatus status;

    if (path->op_tail == NULL || path->op_tail->num_ops + 1 > XR_PATH_BUF_SZ) {
	status = _XrPathNewOpBuf(path);
	if (status)
	    return status;
    }
    _XrPathOpBufAdd(path->op_tail, op);

    if (path->arg_tail == NULL || path->arg_tail->num_pts + num_pts > XR_PATH_BUF_SZ) {
	status = _XrPathNewArgBuf(path);
	if (status)
	    return status;
    }
    _XrPathArgBufAdd(path->arg_tail, pts, num_pts);

    return XrStatusSuccess;
}

static void
_XrPathAddOpBuf(XrPath *path, XrPathOpBuf *op)
{
    op->next = NULL;
    op->prev = path->op_tail;

    if (path->op_tail) {
	path->op_tail->next = op;
    } else {
	path->op_head = op;
    }

    path->op_tail = op;
}

static XrStatus
_XrPathNewOpBuf(XrPath *path)
{
    XrPathOpBuf *op;

    op = _XrPathOpBufCreate();
    if (op == NULL)
	return XrStatusNoMemory;

    _XrPathAddOpBuf(path, op);

    return XrStatusSuccess;
}

static void
_XrPathAddArgBuf(XrPath *path, XrPathArgBuf *arg)
{
    arg->next = NULL;
    arg->prev = path->arg_tail;

    if (path->arg_tail) {
	path->arg_tail->next = arg;
    } else {
	path->arg_head = arg;
    }

    path->arg_tail = arg;
}

static XrStatus
_XrPathNewArgBuf(XrPath *path)
{
    XrPathArgBuf *arg;

    arg = _XrPathArgBufCreate();

    if (arg == NULL)
	return XrStatusNoMemory;

    _XrPathAddArgBuf(path, arg);

    return XrStatusSuccess;
}

static XrPathOpBuf *
_XrPathOpBufCreate(void)
{
    XrPathOpBuf *op;

    op = malloc(sizeof(XrPathOpBuf));

    if (op) {
	op->num_ops = 0;
	op->next = NULL;
    }

    return op;
}

static void
_XrPathOpBufDestroy(XrPathOpBuf *op)
{
    free(op);
}

static void
_XrPathOpBufAdd(XrPathOpBuf *op_buf, XrPathOp op)
{
    op_buf->op[op_buf->num_ops++] = op;
}

static XrPathArgBuf *
_XrPathArgBufCreate(void)
{
    XrPathArgBuf *arg;

    arg = malloc(sizeof(XrPathArgBuf));

    if (arg) {
	arg->num_pts = 0;
	arg->next = NULL;
    }

    return arg;
}

static void
_XrPathArgBufDestroy(XrPathArgBuf *arg)
{
    free(arg);
}

static void
_XrPathArgBufAdd(XrPathArgBuf *arg, XPointFixed *pts, int num_pts)
{
    int i;

    for (i=0; i < num_pts; i++) {
	arg->pt[arg->num_pts++] = pts[i];
    }
}

#define XR_PATH_OP_MAX_ARGS 3

static int num_args[] = 
{
    1, /* XrPathMoveTo */
    1, /* XrPathOpLineTo */
    3, /* XrPathOpCurveTo */
    0, /* XrPathOpClosePath */
};

XrStatus
_XrPathInterpret(XrPath *path, XrPathDirection dir, const XrPathCallbacks *cb, void *closure)
{
    XrStatus status;
    int i, arg;
    XrPathOpBuf *op_buf;
    XrPathOp op;
    XrPathArgBuf *arg_buf = path->arg_head;
    int buf_i = 0;
    XPointFixed pt[XR_PATH_OP_MAX_ARGS];
    XPointFixed current = {0, 0};
    XPointFixed first = {0, 0};
    int has_current = 0;
    int has_edge = 0;
    int step = (dir == XrPathDirectionForward) ? 1 : -1;

    for (op_buf = (dir == XrPathDirectionForward) ? path->op_head : path->op_tail;
	 op_buf;
	 op_buf = (dir == XrPathDirectionForward) ? op_buf->next : op_buf->prev)
    {
	int start, stop;
	if (dir == XrPathDirectionForward) {
	    start = 0;
	    stop = op_buf->num_ops;
	} else {
	    start = op_buf->num_ops - 1;
	    stop = -1;
	}

	for (i=start; i != stop; i += step) {
	    op = op_buf->op[i];

	    if (dir == XrPathDirectionReverse) {
		if (buf_i == 0) {
		    arg_buf = arg_buf->prev;
		    buf_i = arg_buf->num_pts;
		}
		buf_i -= num_args[op];
	    }

	    for (arg = 0; arg < num_args[op]; arg++) {
		pt[arg] = arg_buf->pt[buf_i];
		buf_i++;
		if (buf_i >= arg_buf->num_pts) {
		    arg_buf = arg_buf->next;
		    buf_i = 0;
		}
	    }

	    if (dir == XrPathDirectionReverse) {
		buf_i -= num_args[op];
	    }

	    switch (op) {
	    case XrPathOpMoveTo:
		if (has_edge) {
		    status = (*cb->DoneSubPath) (closure, XrSubPathDoneCap);
		    if (status)
			return status;
		}
		first = pt[0];
		current = pt[0];
		has_current = 1;
		has_edge = 0;
		break;
	    case XrPathOpLineTo:
		if (has_current) {
		    status = (*cb->AddEdge)(closure, &current, &pt[0]);
		    if (status)
			return status;
		    current = pt[0];
		    has_edge = 1;
		} else {
		    first = pt[0];
		    current = pt[0];
		    has_current = 1;
		    has_edge = 0;
		}
		break;
	    case XrPathOpCurveTo:
		if (has_current) {
		    status = (*cb->AddSpline)(closure, &current, &pt[0], &pt[1], &pt[2]);
		    if (status)
			return status;
		    current = pt[2];
		    has_edge = 1;
		} else {
		    first = pt[2];
		    current = pt[2];
		    has_current = 1;
		    has_edge = 0;
		}
		break;
	    case XrPathOpClosePath:
		if (has_edge) {
		    (*cb->AddEdge)(closure, &current, &first);
		    (*cb->DoneSubPath) (closure, XrSubPathDoneJoin);
		}
		current.x = 0;
		current.y = 0;
		first.x = 0;
		first.y = 0;
		has_current = 0;
		has_edge = 0;
		break;
	    }
	}
    }
    if (has_edge)
        (*cb->DoneSubPath) (closure, XrSubPathDoneCap);

    return (*cb->DonePath)(closure);
}


