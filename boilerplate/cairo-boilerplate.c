/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include "cairo-boilerplate.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#if HAVE_FCFINI
#include <fontconfig/fontconfig.h>
#endif

static const char *vector_ignored_tests[] = {
    /* We can't match the results of tests that depend on
     * CAIRO_ANTIALIAS_NONE/SUBPIXEL for vector backends
     * (nor do we care). */
    "ft-text-antialias-none",
    "rectangle-rounding-error",
    "text-antialias-gray",
    "text-antialias-none",
    "text-antialias-subpixel",
    "unantialiased-shapes",
    NULL
};

const char *
_cairo_test_content_name (cairo_content_t content)
{
    /* For the purpose of the content name, we don't distinguish the
     * flattened content value.
     */
    if (content == CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED)
	content = CAIRO_CONTENT_COLOR_ALPHA;

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	return "rgb24";
    case CAIRO_CONTENT_COLOR_ALPHA:
	return "argb32";
    case CAIRO_CONTENT_ALPHA:
    default:
	assert (0); /* not reached */
	return "---";
    }
}

static cairo_surface_t *
create_image_surface (const char	 *name,
		      cairo_content_t	  content,
		      int		  width,
		      int		  height,
		      void		**closure)
{
    cairo_format_t format;
    *closure = NULL;

    if (content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = CAIRO_FORMAT_ARGB32;
    } else if (content == CAIRO_CONTENT_COLOR) {
	format = CAIRO_FORMAT_RGB24;
    } else {
	assert (0); /* not reached */
	return NULL;
    }

    return cairo_image_surface_create (format, width, height);
}

#ifdef CAIRO_HAS_TEST_SURFACES

#include "test-fallback-surface.h"
#include "test-meta-surface.h"
#include "test-paginated-surface.h"

static cairo_surface_t *
create_test_fallback_surface (const char	 *name,
			      cairo_content_t	  content,
			      int		  width,
			      int		  height,
			      void		**closure)
{
    *closure = NULL;
    return _test_fallback_surface_create (content, width, height);
}

static cairo_surface_t *
create_test_meta_surface (const char		 *name,
			  cairo_content_t	  content,
			  int			  width,
			  int			  height,
			  void			**closure)
{
    *closure = NULL;
    return _test_meta_surface_create (content, width, height);
}

static const cairo_user_data_key_t test_paginated_closure_key;

typedef struct {
    unsigned char *data;
    cairo_content_t content;
    int width;
    int height;
    int stride;
} test_paginated_closure_t;

static cairo_surface_t *
create_test_paginated_surface (const char	 *name,
			       cairo_content_t	  content,
			       int		  width,
			       int		  height,
			       void		**closure)
{
    test_paginated_closure_t *tpc;
    cairo_surface_t *surface;

    *closure = tpc = xmalloc (sizeof (test_paginated_closure_t));

    tpc->content = content;
    tpc->width = width;
    tpc->height = height;
    tpc->stride = width * 4;

    tpc->data = xcalloc (tpc->stride * height, 1);

    surface = _test_paginated_surface_create_for_data (tpc->data,
						       tpc->content,
						       tpc->width,
						       tpc->height,
						       tpc->stride);

    cairo_surface_set_user_data (surface, &test_paginated_closure_key,
				 tpc, NULL);

    return surface;
}

/* The only reason we go through all these machinations to write a PNG
 * image is to _really ensure_ that the data actually landed in our
 * buffer through the paginated surface to the test_paginated_surface.
 *
 * If we didn't implement this function then the default
 * cairo_surface_write_to_png would result in the paginated_surface's
 * acquire_source_image function replaying the meta-surface to an
 * intermediate image surface. And in that case the
 * test_paginated_surface would not be involved and wouldn't be
 * tested.
 */
static cairo_status_t
test_paginated_write_to_png (cairo_surface_t *surface,
			     const char	     *filename)
{
    cairo_surface_t *image;
    cairo_format_t format;
    test_paginated_closure_t *tpc;

    tpc = cairo_surface_get_user_data (surface, &test_paginated_closure_key);

    switch (tpc->content) {
    case CAIRO_CONTENT_COLOR:
	format = CAIRO_FORMAT_RGB24;
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = CAIRO_FORMAT_ARGB32;
	break;
    case CAIRO_CONTENT_ALPHA:
    default:
	assert (0); /* not reached */
	return CAIRO_STATUS_NO_MEMORY;
    }

    image = cairo_image_surface_create_for_data (tpc->data,
						 format,
						 tpc->width,
						 tpc->height,
						 tpc->stride);

    cairo_surface_write_to_png (image, filename);

    cairo_surface_destroy (image);

    return CAIRO_STATUS_SUCCESS;
}

static void
cleanup_test_paginated (void *closure)
{
    test_paginated_closure_t *tpc = closure;

    free (tpc->data);
    free (tpc);
}

#endif

#ifdef CAIRO_HAS_GLITZ_SURFACE
#include <glitz.h>
#include <cairo-glitz.h>

static const cairo_user_data_key_t glitz_closure_key;

typedef struct _glitz_target_closure_base {
    int width;
    int height;
    cairo_content_t content;
} glitz_target_closure_base_t;

#if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
#include <glitz-glx.h>

typedef struct _glitz_glx_target_closure {
    glitz_target_closure_base_t base;
    Display        *dpy;
    int             scr;
    Window          win;
} glitz_glx_target_closure_t;

static glitz_surface_t *
create_glitz_glx_surface (glitz_format_name_t	      formatname,
			  int			      width,
			  int			      height,
			  glitz_glx_target_closure_t *closure)
{
    Display                 * dpy = closure->dpy;
    int                       scr = closure->scr;
    glitz_drawable_format_t   templ;
    glitz_drawable_format_t * dformat = NULL;
    unsigned long             mask;
    glitz_drawable_t        * drawable = NULL;
    glitz_format_t          * format;
    glitz_surface_t         * sr;

    XSizeHints                xsh;
    XSetWindowAttributes      xswa;
    XVisualInfo             * vinfo;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    glitz_glx_init (NULL);

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    /* Try for a pbuffer first */
    if (!getenv("CAIRO_TEST_FORCE_GLITZ_WINDOW"))
	dformat = glitz_glx_find_pbuffer_format (dpy, scr, mask, &templ, 0);

    if (dformat) {
	closure->win = None;

	drawable = glitz_glx_create_pbuffer_drawable (dpy, scr, dformat,
						      width, height);
	if (!drawable)
	    goto FAIL;
    } else {
	/* No pbuffer, try window */
	dformat = glitz_glx_find_window_format (dpy, scr, mask, &templ, 0);

	if (!dformat)
	    goto FAIL;

	vinfo = glitz_glx_get_visual_info_from_format(dpy,
						      DefaultScreen(dpy),
						      dformat);

	if (!vinfo)
	    goto FAIL;

	xsh.flags = PSize;
	xsh.x = 0;
	xsh.y = 0;
	xsh.width = width;
	xsh.height = height;

	xswa.colormap = XCreateColormap (dpy, RootWindow(dpy, scr),
					 vinfo->visual, AllocNone);
	closure->win = XCreateWindow (dpy, RootWindow(dpy, scr),
				      xsh.x, xsh.y, xsh.width, xsh.height,
				      0, vinfo->depth, CopyFromParent,
				      vinfo->visual, CWColormap, &xswa);
	XFree (vinfo);

	drawable =
	    glitz_glx_create_drawable_for_window (dpy, scr,
						  dformat, closure->win,
						  width, height);

	if (!drawable)
	    goto DESTROY_WINDOW;
    }

    format = glitz_find_standard_format (drawable, formatname);
    if (!format)
	goto DESTROY_DRAWABLE;

    sr = glitz_surface_create (drawable, format, width, height, 0, NULL);
    if (!sr)
	goto DESTROY_DRAWABLE;

    if (closure->win == None || dformat->doublebuffer) {
	glitz_surface_attach (sr, drawable, GLITZ_DRAWABLE_BUFFER_BACK_COLOR);
    } else {
	XMapWindow (closure->dpy, closure->win);
	glitz_surface_attach (sr, drawable, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);
    }

    glitz_drawable_destroy (drawable);

    return sr;
 DESTROY_DRAWABLE:
    glitz_drawable_destroy (drawable);
 DESTROY_WINDOW:
    if (closure->win)
	XDestroyWindow (dpy, closure->win);
 FAIL:
    return NULL;
}

static cairo_surface_t *
create_cairo_glitz_glx_surface (const char	 *name,
				cairo_content_t   content,
				int		  width,
				int		  height,
				void		**closure)
{
    int width = width;
    int height = height;
    glitz_glx_target_closure_t *gxtc;
    glitz_surface_t  * glitz_surface;
    cairo_surface_t  * surface;

    *closure = gxtc = xmalloc (sizeof (glitz_glx_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    gxtc->dpy = XOpenDisplay (getenv("CAIRO_TEST_GLITZ_DISPLAY"));
    if (!gxtc->dpy) {
	CAIRO_BOILERPLATE_LOG ("Failed to open display: %s\n", XDisplayName(0));
	goto FAIL;
    }

    XSynchronize (gxtc->dpy, 1);

    gxtc->scr = DefaultScreen(gxtc->dpy);

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	glitz_surface = create_glitz_glx_surface (GLITZ_STANDARD_RGB24, width, height, gxtc);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = create_glitz_glx_surface (GLITZ_STANDARD_ARGB32, width, height, gxtc);
	break;
    default:
	CAIRO_BOILERPLATE_LOG ("Invalid content for glitz-glx test: %d\n", content);
	goto FAIL_CLOSE_DISPLAY;
    }
    if (!glitz_surface) {
	CAIRO_BOILERPLATE_LOG ("Failed to create glitz-glx surface\n");
	goto FAIL_CLOSE_DISPLAY;
    }

    surface = cairo_glitz_surface_create (glitz_surface);

    gxtc->base.width = width;
    gxtc->base.height = height;
    gxtc->base.content = content;
    cairo_surface_set_user_data (surface, &glitz_closure_key,
				 gxtc, NULL);

    return surface;

 FAIL_CLOSE_DISPLAY:
    XCloseDisplay (gxtc->dpy);
 FAIL:
    return NULL;
}

static void
cleanup_cairo_glitz_glx (void *closure)
{
    glitz_glx_target_closure_t *gxtc = closure;

    glitz_glx_fini ();

    if (gxtc->win)
	XDestroyWindow (gxtc->dpy, gxtc->win);

    XCloseDisplay (gxtc->dpy);

    free (gxtc);
}

#endif /* CAIRO_CAN_TEST_GLITZ_GLX_SURFACE */

#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
#include <glitz-agl.h>

typedef struct _glitz_agl_target_closure {
    glitz_target_closure_base_t base;
} glitz_agl_target_closure_t;

static glitz_surface_t *
create_glitz_agl_surface (glitz_format_name_t formatname,
			  int width, int height,
			  glitz_agl_target_closure_t *closure)
{
    glitz_drawable_format_t *dformat;
    glitz_drawable_format_t templ;
    glitz_drawable_t *gdraw;
    glitz_format_t *format;
    glitz_surface_t *sr = NULL;
    unsigned long mask;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    dformat = glitz_agl_find_pbuffer_format (mask, &templ, 0);
    if (!dformat) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to find pbuffer format for template.");
	goto FAIL;
    }

    gdraw = glitz_agl_create_pbuffer_drawable (dformat, width, height);
    if (!gdraw) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to create pbuffer drawable.");
	goto FAIL;
    }

    format = glitz_find_standard_format (gdraw, formatname);
    if (!format) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to find standard format for drawable.");
	goto DESTROY_DRAWABLE;
    }

    sr = glitz_surface_create (gdraw, format, width, height, 0, NULL);
    if (!sr) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to create a surface.");
	goto DESTROY_DRAWABLE;
    }

    glitz_surface_attach (sr, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

 DESTROY_DRAWABLE:
    glitz_drawable_destroy (gdraw);

 FAIL:
    return sr; /* will be NULL unless we create it and attach */
}

static cairo_surface_t *
create_cairo_glitz_agl_surface (const char	 *name,
				cairo_content_t   content,
				int		  width,
				int		  height,
				void		**closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface;
    glitz_agl_target_closure_t *aglc;

    glitz_agl_init ();

    *closure = aglc = xmalloc (sizeof (glitz_agl_target_closure_t));

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	glitz_surface = create_glitz_agl_surface (GLITZ_STANDARD_RGB24, width, height, NULL);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = create_glitz_agl_surface (GLITZ_STANDARD_ARGB32, width, height, NULL);
	break;
    default:
	CAIRO_BOILERPLATE_LOG ("Invalid content for glitz-agl test: %d\n", content);
	goto FAIL;
    }

    if (!glitz_surface)
	goto FAIL;

    surface = cairo_glitz_surface_create (glitz_surface);

    aglc->base.width = width;
    aglc->base.height = height;
    aglc->base.content = content;
    cairo_surface_set_user_data (surface, &glitz_closure_key, aglc, NULL);

    return surface;

 FAIL:
    return NULL;
}

static void
cleanup_cairo_glitz_agl (void *closure)
{
    free (closure);
    glitz_agl_fini ();
}

#endif /* CAIRO_CAN_TEST_GLITZ_AGL_SURFACE */

#if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
#include <glitz-wgl.h>

typedef struct _glitz_wgl_target_closure {
    glitz_target_closure_base_t base;
} glitz_wgl_target_closure_t;

static glitz_surface_t *
create_glitz_wgl_surface (glitz_format_name_t formatname,
			  int width, int height,
			  glitz_wgl_target_closure_t *closure)
{
    glitz_drawable_format_t *dformat;
    glitz_drawable_format_t templ;
    glitz_drawable_t *gdraw;
    glitz_format_t *format;
    glitz_surface_t *sr = NULL;
    unsigned long mask;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    dformat = glitz_wgl_find_pbuffer_format (mask, &templ, 0);
    if (!dformat) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to find pbuffer format for template.");
	goto FAIL;
    }

    gdraw = glitz_wgl_create_pbuffer_drawable (dformat, width, height);
    if (!gdraw) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to create pbuffer drawable.");
	goto FAIL;
    }

    format = glitz_find_standard_format (gdraw, formatname);
    if (!format) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to find standard format for drawable.");
	goto DESTROY_DRAWABLE;
    }

    sr = glitz_surface_create (gdraw, format, width, height, 0, NULL);
    if (!sr) {
	CAIRO_BOILERPLATE_LOG ("Glitz failed to create a surface.");
	goto DESTROY_DRAWABLE;
    }

    glitz_surface_attach (sr, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

 DESTROY_DRAWABLE:
    glitz_drawable_destroy (gdraw);

 FAIL:
    return sr; /* will be NULL unless we create it and attach */
}

static cairo_surface_t *
create_cairo_glitz_wgl_surface (const char	 *name,
				cairo_content_t	  content,
				int		  width,
				int		  height,
				void		**closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface;
    glitz_wgl_target_closure_t *wglc;

    glitz_wgl_init (NULL);

    *closure = wglc = xmalloc (sizeof (glitz_wgl_target_closure_t));

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	glitz_surface = create_glitz_wgl_surface (GLITZ_STANDARD_RGB24, width, height, NULL);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = create_glitz_wgl_surface (GLITZ_STANDARD_ARGB32, width, height, NULL);
	break;
    default:
	CAIRO_BOILERPLATE_LOG ("Invalid content for glitz-wgl test: %d\n", content);
	goto FAIL;
    }

    if (!glitz_surface)
	goto FAIL;

    surface = cairo_glitz_surface_create (glitz_surface);

    wglc->base.width = width;
    wglc->base.height = height;
    wglc->base.content = content;
    cairo_surface_set_user_data (surface, &glitz_closure_key, wglc, NULL);

    return surface;

 FAIL:
    return NULL;
}

static void
cleanup_cairo_glitz_wgl (void *closure)
{
    free (closure);
    glitz_wgl_fini ();
}

#endif /* CAIRO_CAN_TEST_GLITZ_WGL_SURFACE */

#endif /* CAIRO_HAS_GLITZ_SURFACE */

#if 0 && CAIRO_HAS_QUARTZ_SURFACE
static cairo_surface_t *
create_quartz_surface (int width, int height, void **closure)
{
#error Not yet implemented
}

static void
cleanup_quartz (void *closure)
{
#error Not yet implemented
}
#endif

/* Testing the win32 surface isn't interesting, since for
 * ARGB images it just chains to the image backend
 */
#if CAIRO_HAS_WIN32_SURFACE
#include "cairo-win32.h"
typedef struct _win32_target_closure
{
  HDC dc;
  HBITMAP bmp;
} win32_target_closure_t;

static cairo_surface_t *
create_win32_surface (const char	 *name,
		      cairo_content_t	  content,
		      int		  width,
		      int		  height,
		      void		**closure)
{
    int width = width;
    int height = height;

    BITMAPINFO bmpInfo;
    unsigned char *bits = NULL;
    win32_target_closure_t *data = malloc(sizeof(win32_target_closure_t));
    *closure = data;

    data->dc = CreateCompatibleDC(NULL);

    /* initialize the bitmapinfoheader */
    memset(&bmpInfo.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
    bmpInfo.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -height;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 24;
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    /* create a DIBSection */
    data->bmp = CreateDIBSection(data->dc, &bmpInfo, DIB_RGB_COLORS, (void**)&bits, NULL, 0);

    /* Flush GDI to make sure the DIBSection is actually created */
    GdiFlush();

    /* Select the bitmap in to the DC */
    SelectObject(data->dc, data->bmp);

    return cairo_win32_surface_create(data->dc);
}

static void
cleanup_win32 (void *closure)
{
  win32_target_closure_t *data = (win32_target_closure_t*)closure;
  DeleteObject(data->bmp);
  DeleteDC(data->dc);

  free(closure);
}
#endif

#if CAIRO_HAS_XCB_SURFACE
#include "cairo-xcb-xrender.h"
typedef struct _xcb_target_closure
{
    XCBConnection *c;
    XCBDRAWABLE drawable;
} xcb_target_closure_t;

/* XXX: This is a nasty hack. Something like this should be in XCB's
 * bindings for Render, not here in this test. */
static XCBRenderPICTFORMINFO
_format_from_cairo(XCBConnection *c, cairo_format_t fmt)
{
    XCBRenderPICTFORMINFO ret = {{ 0 }};
    struct tmpl_t {
	XCBRenderDIRECTFORMAT direct;
	CARD8 depth;
    };
    static const struct tmpl_t templates[] = {
	/* CAIRO_FORMAT_ARGB32 */
	{
	    {
		16, 0xff,
		8,  0xff,
		0,  0xff,
		24, 0xff
	    },
	    32
	},
	/* CAIRO_FORMAT_RGB24 */
	{
	    {
		16, 0xff,
		8,  0xff,
		0,  0xff,
		0,  0x00
	    },
	    24
	},
	/* CAIRO_FORMAT_A8 */
	{
	    {
		0,  0x00,
		0,  0x00,
		0,  0x00,
		0,  0xff
	    },
	    8
	},
	/* CAIRO_FORMAT_A1 */
	{
	    {
		0,  0x00,
		0,  0x00,
		0,  0x00,
		0,  0x01
	    },
	    1
	},
    };
    const struct tmpl_t *tmpl;
    XCBRenderQueryPictFormatsRep *r;
    XCBRenderPICTFORMINFOIter fi;

    if(fmt < 0 || fmt >= (sizeof(templates) / sizeof(*templates)))
	return ret;
    tmpl = templates + fmt;

    r = XCBRenderQueryPictFormatsReply(c, XCBRenderQueryPictFormats(c), 0);
    if(!r)
	return ret;

    for(fi = XCBRenderQueryPictFormatsFormatsIter(r); fi.rem; XCBRenderPICTFORMINFONext(&fi))
    {
	const XCBRenderDIRECTFORMAT *t, *f;
	if(fi.data->type != XCBRenderPictTypeDirect)
	    continue;
	if(fi.data->depth != tmpl->depth)
	    continue;
	t = &tmpl->direct;
	f = &fi.data->direct;
	if(t->red_mask && (t->red_mask != f->red_mask || t->red_shift != f->red_shift))
	    continue;
	if(t->green_mask && (t->green_mask != f->green_mask || t->green_shift != f->green_shift))
	    continue;
	if(t->blue_mask && (t->blue_mask != f->blue_mask || t->blue_shift != f->blue_shift))
	    continue;
	if(t->alpha_mask && (t->alpha_mask != f->alpha_mask || t->alpha_shift != f->alpha_shift))
	    continue;

	ret = *fi.data;
    }

    free(r);
    return ret;
}

static cairo_surface_t *
create_xcb_surface (const char		 *name,
		    cairo_content_t	  content,
		    int			  width,
		    int			  height,
		    void		**closure)
{
    int width = width;
    int height = height;
    XCBSCREEN *root;
    xcb_target_closure_t *xtc;
    cairo_surface_t *surface;
    XCBConnection *c;
    XCBRenderPICTFORMINFO render_format;
    cairo_format_t format;

    *closure = xtc = xmalloc (sizeof (xcb_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->c = c = XCBConnect(NULL,NULL);
    if (c == NULL) {
	CAIRO_BOILERPLATE_LOG ("Failed to connect to X server through XCB\n");
	return NULL;
    }

    root = XCBSetupRootsIter(XCBGetSetup(c)).data;

    xtc->drawable.pixmap = XCBPIXMAPNew (c);
    {
	XCBDRAWABLE root_drawable;
	root_drawable.window = root->root;
	XCBCreatePixmap (c, 32, xtc->drawable.pixmap, root_drawable,
			 width, height);
    }

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	format = CAIRO_FORMAT_RGB24;
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = CAIRO_FORMAT_ARGB32;
	break;
    default:
	CAIRO_BOILERPLATE_LOG ("Invalid content for XCB test: %d\n", content);
	return NULL;
    }

    render_format = _format_from_cairo (c, format);
    if (render_format.id.xid == 0)
	return NULL;
    surface = cairo_xcb_surface_create_with_xrender_format (c, xtc->drawable, root,
							    &render_format,
							    width, height);

    return surface;
}

static void
cleanup_xcb (void *closure)
{
    xcb_target_closure_t *xtc = closure;

    XCBFreePixmap (xtc->c, xtc->drawable.pixmap);
    XCBDisconnect (xtc->c);
    free (xtc);
}
#endif

#if CAIRO_HAS_XLIB_SURFACE
#include "cairo-xlib-xrender.h"
typedef struct _xlib_target_closure
{
    Display *dpy;
    Pixmap pixmap;
} xlib_target_closure_t;

static cairo_surface_t *
create_xlib_surface (const char		 *name,
		     cairo_content_t	  content,
		     int		  width,
		     int		  height,
		     void		**closure)
{
    xlib_target_closure_t *xtc;
    cairo_surface_t *surface;
    Display *dpy;
    XRenderPictFormat *xrender_format;

    *closure = xtc = xmalloc (sizeof (xlib_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->dpy = dpy = XOpenDisplay (NULL);
    if (xtc->dpy == NULL) {
	CAIRO_BOILERPLATE_LOG ("Failed to open display: %s\n", XDisplayName(0));
	return NULL;
    }

    XSynchronize (xtc->dpy, 1);

    /* XXX: Currently we don't do any xlib testing when the X server
     * doesn't have the Render extension. We could do better here,
     * (perhaps by converting the tests from ARGB32 to RGB24). One
     * step better would be to always test the non-Render fallbacks
     * for each test even if the server does have the Render
     * extension. That would probably be through another
     * cairo_test_target which would use an extended version of
     * cairo_test_xlib_disable_render.  */
    switch (content) {
    case CAIRO_CONTENT_COLOR_ALPHA:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardARGB32);
	break;
    case CAIRO_CONTENT_COLOR:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardRGB24);
	break;
    case CAIRO_CONTENT_ALPHA:
    default:
	CAIRO_BOILERPLATE_LOG ("Invalid content for xlib test: %d\n", content);
	return NULL;
    }
    if (xrender_format == NULL) {
	CAIRO_BOILERPLATE_LOG ("X server does not have the Render extension.\n");
	return NULL;
    }

    xtc->pixmap = XCreatePixmap (dpy, DefaultRootWindow (dpy),
				 width, height, xrender_format->depth);

    surface = cairo_xlib_surface_create_with_xrender_format (dpy, xtc->pixmap,
							     DefaultScreenOfDisplay (dpy),
							     xrender_format,
							     width, height);
    return surface;
}

static void
cleanup_xlib (void *closure)
{
    xlib_target_closure_t *xtc = closure;

    XFreePixmap (xtc->dpy, xtc->pixmap);
    XCloseDisplay (xtc->dpy);
    free (xtc);
}
#endif

#if CAIRO_HAS_BEOS_SURFACE
/* BeOS test functions are external as they need to be C++ */
#include "cairo-test-beos.h"
#endif

#if CAIRO_HAS_DIRECTFB_SURFACE
#include "cairo-test-directfb.h"
#endif

#if CAIRO_HAS_PS_SURFACE
#include "cairo-ps.h"

cairo_user_data_key_t	ps_closure_key;

typedef struct _ps_target_closure
{
    char		*filename;
    int			 width;
    int			 height;
    cairo_surface_t	*target;
} ps_target_closure_t;

static cairo_surface_t *
create_ps_surface (const char		 *name,
		   cairo_content_t	  content,
		   int			  width,
		   int			  height,
		   void			**closure)
{
    ps_target_closure_t	*ptc;
    cairo_surface_t *surface;
    int i;

    for (i = 0; vector_ignored_tests[i] != NULL; i++)
	if (strcmp (name, vector_ignored_tests[i]) == 0)
	    return NULL;

    /* Sanitize back to a real cairo_content_t value. */
    if (content == CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED)
	content = CAIRO_CONTENT_COLOR_ALPHA;

    *closure = ptc = xmalloc (sizeof (ps_target_closure_t));

    xasprintf (&ptc->filename, "%s-ps-%s-out.ps",
	       name, _cairo_test_content_name (content));

    ptc->width = width;
    ptc->height = height;

    surface = cairo_ps_surface_create (ptc->filename, width, height);
    if (cairo_surface_status (surface)) {
	free (ptc->filename);
	free (ptc);
	return NULL;
    }
    cairo_surface_set_fallback_resolution (surface, 72., 72.);

    if (content == CAIRO_CONTENT_COLOR) {
	ptc->target = surface;
	surface = cairo_surface_create_similar (ptc->target,
						CAIRO_CONTENT_COLOR,
						width, height);
    } else {
	ptc->target = NULL;
    }

    cairo_surface_set_user_data (surface, &ps_closure_key, ptc, NULL);

    return surface;
}

static cairo_status_t
ps_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    ps_target_closure_t *ptc = cairo_surface_get_user_data (surface, &ps_closure_key);
    char    command[4096];

    /* Both surface and ptc->target were originally created at the
     * same dimensions. We want a 1:1 copy here, so we first clear any
     * device offset on surface.
     *
     * In a more realistic use case of device offsets, the target of
     * this copying would be of a different size than the source, and
     * the offset would be desirable during the copy operation. */
    cairo_surface_set_device_offset (surface, 0, 0);

    if (ptc->target) {
	cairo_t *cr;
	cr = cairo_create (ptc->target);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
	cairo_show_page (cr);
	cairo_destroy (cr);

	cairo_surface_finish (surface);
	surface = ptc->target;
    }

    cairo_surface_finish (surface);
    sprintf (command, "gs -q -r72 -g%dx%d -dSAFER -dBATCH -dNOPAUSE -sDEVICE=pngalpha -sOutputFile=%s %s",
	     ptc->width, ptc->height, filename, ptc->filename);
    if (system (command) == 0)
	return CAIRO_STATUS_SUCCESS;
    return CAIRO_STATUS_WRITE_ERROR;
}

static void
cleanup_ps (void *closure)
{
    ps_target_closure_t *ptc = closure;
    if (ptc->target)
	cairo_surface_destroy (ptc->target);
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_PS_SURFACE */

#if CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE
#include "cairo-pdf.h"

cairo_user_data_key_t pdf_closure_key;

typedef struct _pdf_target_closure
{
    char		*filename;
    int			 width;
    int			 height;
    cairo_surface_t	*target;
} pdf_target_closure_t;

static cairo_surface_t *
create_pdf_surface (const char		 *name,
		    cairo_content_t	  content,
		    int			  width,
		    int			  height,
		    void		**closure)
{
    int width = width;
    int height = height;
    pdf_target_closure_t *ptc;
    cairo_surface_t *surface;
    int i;

    for (i = 0; vector_ignored_tests[i] != NULL; i++)
	if (strcmp (name, vector_ignored_tests[i]) == 0)
	    return NULL;

    /* Sanitize back to a real cairo_content_t value. */
    if (content == CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED)
	content = CAIRO_CONTENT_COLOR_ALPHA;

    *closure = ptc = xmalloc (sizeof (pdf_target_closure_t));

    ptc->width = width;
    ptc->height = height;

    xasprintf (&ptc->filename, "%s-pdf-%s-out.pdf",
	       name, _cairo_test_content_name (content));

    surface = cairo_pdf_surface_create (ptc->filename, width, height);
    if (cairo_surface_status (surface)) {
	free (ptc->filename);
	free (ptc);
	return NULL;
    }
    cairo_surface_set_fallback_resolution (surface, 72., 72.);

    if (content == CAIRO_CONTENT_COLOR) {
	ptc->target = surface;
	surface = cairo_surface_create_similar (ptc->target,
						CAIRO_CONTENT_COLOR,
						width, height);
    } else {
	ptc->target = NULL;
    }

    cairo_surface_set_user_data (surface, &pdf_closure_key, ptc, NULL);

    return surface;
}

static cairo_status_t
pdf_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    pdf_target_closure_t *ptc = cairo_surface_get_user_data (surface, &pdf_closure_key);
    char    command[4096];

    /* Both surface and ptc->target were originally created at the
     * same dimensions. We want a 1:1 copy here, so we first clear any
     * device offset on surface.
     *
     * In a more realistic use case of device offsets, the target of
     * this copying would be of a different size than the source, and
     * the offset would be desirable during the copy operation. */
    cairo_surface_set_device_offset (surface, 0, 0);

    if (ptc->target) {
	cairo_t *cr;
	cr = cairo_create (ptc->target);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
	cairo_show_page (cr);
	cairo_destroy (cr);

	cairo_surface_finish (surface);
	surface = ptc->target;
    }

    cairo_surface_finish (surface);
    sprintf (command, "./pdf2png %s %s 1",
	     ptc->filename, filename);

    if (system (command) != 0)
	return CAIRO_STATUS_WRITE_ERROR;

    return CAIRO_STATUS_SUCCESS;
}

static void
cleanup_pdf (void *closure)
{
    pdf_target_closure_t *ptc = closure;
    if (ptc->target)
	cairo_surface_destroy (ptc->target);
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE */

#if CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE
#include "cairo-svg.h"

cairo_user_data_key_t	svg_closure_key;

typedef struct _svg_target_closure
{
    char    *filename;
    int	    width, height;
    cairo_surface_t	*target;
} svg_target_closure_t;

static cairo_surface_t *
create_svg_surface (const char		 *name,
		    cairo_content_t	  content,
		    int			  width,
		    int			  height,
		    void		**closure)
{
    int width = width;
    int height = height;
    int i;
    svg_target_closure_t *ptc;
    cairo_surface_t *surface;

    for (i = 0; vector_ignored_tests[i] != NULL; i++)
	if (strcmp (name, vector_ignored_tests[i]) == 0)
	    return NULL;

    *closure = ptc = xmalloc (sizeof (svg_target_closure_t));

    ptc->width = width;
    ptc->height = height;

    xasprintf (&ptc->filename, "%s-svg-%s-out.svg",
	       name, _cairo_test_content_name (content));

    surface = cairo_svg_surface_create (ptc->filename, width, height);
    if (cairo_surface_status (surface)) {
	free (ptc->filename);
	free (ptc);
	return NULL;
    }
    cairo_surface_set_fallback_resolution (surface, 72., 72.);

    if (content == CAIRO_CONTENT_COLOR) {
	ptc->target = surface;
	surface = cairo_surface_create_similar (ptc->target,
						CAIRO_CONTENT_COLOR,
						width, height);
    } else {
	ptc->target = NULL;
    }

    cairo_surface_set_user_data (surface, &svg_closure_key, ptc, NULL);

    return surface;
}

static cairo_status_t
svg_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    svg_target_closure_t *ptc = cairo_surface_get_user_data (surface, &svg_closure_key);
    char    command[4096];

    /* Both surface and ptc->target were originally created at the
     * same dimensions. We want a 1:1 copy here, so we first clear any
     * device offset on surface.
     *
     * In a more realistic use case of device offsets, the target of
     * this copying would be of a different size than the source, and
     * the offset would be desirable during the copy operation. */
    cairo_surface_set_device_offset (surface, 0, 0);

    if (ptc->target) {
	cairo_t *cr;
	cr = cairo_create (ptc->target);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
	cairo_show_page (cr);
	cairo_destroy (cr);

	cairo_surface_finish (surface);
	surface = ptc->target;
    }

    cairo_surface_finish (surface);
    sprintf (command, "./svg2png %s %s",
	     ptc->filename, filename);

    if (system (command) != 0)
	return CAIRO_STATUS_WRITE_ERROR;

    return CAIRO_STATUS_SUCCESS;
}

static void
cleanup_svg (void *closure)
{
    svg_target_closure_t *ptc = closure;
    if (ptc->target)
	cairo_surface_destroy (ptc->target);
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE */

cairo_test_target_t targets[] =
{
    { "image", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR_ALPHA,
      create_image_surface, cairo_surface_write_to_png, NULL},
    { "image", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR,
      create_image_surface, cairo_surface_write_to_png, NULL},
#ifdef CAIRO_HAS_TEST_SURFACES
    { "test-fallback", CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
      CAIRO_CONTENT_COLOR_ALPHA,
      create_test_fallback_surface, cairo_surface_write_to_png, NULL },
    { "test-fallback", CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
      CAIRO_CONTENT_COLOR,
      create_test_fallback_surface, cairo_surface_write_to_png, NULL },
    { "test-meta", CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
      CAIRO_CONTENT_COLOR_ALPHA,
      create_test_meta_surface, cairo_surface_write_to_png, NULL },
    { "test-meta", CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
      CAIRO_CONTENT_COLOR,
      create_test_meta_surface, cairo_surface_write_to_png, NULL },
    { "test-paginated", CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,
      CAIRO_CONTENT_COLOR_ALPHA,
      create_test_paginated_surface,
      test_paginated_write_to_png,
      cleanup_test_paginated },
    { "test-paginated", CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,
      CAIRO_CONTENT_COLOR,
      create_test_paginated_surface,
      test_paginated_write_to_png,
      cleanup_test_paginated },
#endif
#ifdef CAIRO_HAS_GLITZ_SURFACE
#if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
    { "glitz-glx", CAIRO_SURFACE_TYPE_GLITZ,CAIRO_CONTENT_COLOR_ALPHA,
      create_cairo_glitz_glx_surface, cairo_surface_write_to_png,
      cleanup_cairo_glitz_glx },
    { "glitz-glx", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR,
      create_cairo_glitz_glx_surface, cairo_surface_write_to_png,
      cleanup_cairo_glitz_glx },
#endif
#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
    { "glitz-agl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA,
      create_cairo_glitz_agl_surface, cairo_surface_write_to_png,
      cleanup_cairo_glitz_agl },
    { "glitz-agl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR,
      create_cairo_glitz_agl_surface, cairo_surface_write_to_png,
      cleanup_cairo_glitz_agl },
#endif
#if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
    { "glitz-wgl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA,
      create_cairo_glitz_wgl_surface, cairo_surface_write_to_png,
      cleanup_cairo_glitz_wgl },
    { "glitz-wgl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR,
      create_cairo_glitz_wgl_surface, cairo_surface_write_to_png,
      cleanup_cairo_glitz_wgl },
#endif
#endif /* CAIRO_HAS_GLITZ_SURFACE */
#if 0 && CAIRO_HAS_QUARTZ_SURFACE
    { "quartz", CAIRO_SURFACE_TYPE_QUARTZ, CAIRO_CONTENT_COLOR,
      create_quartz_surface, cairo_surface_write_to_png,
      cleanup_quartz },
#endif
#if CAIRO_HAS_WIN32_SURFACE
    { "win32", CAIRO_SURFACE_TYPE_WIN32, CAIRO_CONTENT_COLOR,
      create_win32_surface, cairo_surface_write_to_png, cleanup_win32 },
#endif
#if CAIRO_HAS_XCB_SURFACE
    { "xcb", CAIRO_SURFACE_TYPE_XCB, CAIRO_CONTENT_COLOR_ALPHA,
      create_xcb_surface, cairo_surface_write_to_png, cleanup_xcb},
#endif
#if CAIRO_HAS_XLIB_SURFACE
    { "xlib", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR_ALPHA,
      create_xlib_surface, cairo_surface_write_to_png, cleanup_xlib},
    { "xlib", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR,
      create_xlib_surface, cairo_surface_write_to_png, cleanup_xlib},
#endif
#if CAIRO_HAS_PS_SURFACE
    { "ps", CAIRO_SURFACE_TYPE_PS,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED,
      create_ps_surface, ps_surface_write_to_png, cleanup_ps },

    /* XXX: We expect type image here only due to a limitation in
     * the current PS/meta-surface code. A PS surface is
     * "naturally" COLOR_ALPHA, so the COLOR-only variant goes
     * through create_similar in create_ps_surface which results
     * in the similar surface being used as a source. We do not yet
     * have source support for PS/meta-surfaces, so the
     * create_similar path for all paginated surfaces currently
     * returns an image surface.*/
    { "ps", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR,
      create_ps_surface, ps_surface_write_to_png, cleanup_ps },
#endif
#if CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE
    { "pdf", CAIRO_SURFACE_TYPE_PDF,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED,
      create_pdf_surface, pdf_surface_write_to_png, cleanup_pdf },

    /* XXX: We expect type image here only due to a limitation in
     * the current PDF/meta-surface code. A PDF surface is
     * "naturally" COLOR_ALPHA, so the COLOR-only variant goes
     * through create_similar in create_pdf_surface which results
     * in the similar surface being used as a source. We do not yet
     * have source support for PDF/meta-surfaces, so the
     * create_similar path for all paginated surfaces currently
     * returns an image surface.*/
    { "pdf", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR,
      create_pdf_surface, pdf_surface_write_to_png, cleanup_pdf },
#endif
#if CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE
    { "svg", CAIRO_SURFACE_TYPE_SVG, CAIRO_CONTENT_COLOR_ALPHA,
      create_svg_surface, svg_surface_write_to_png, cleanup_svg },
    { "svg", CAIRO_INTERNAL_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR,
      create_svg_surface, svg_surface_write_to_png, cleanup_svg },
#endif
#if CAIRO_HAS_BEOS_SURFACE
    { "beos", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR,
      create_beos_surface, cairo_surface_write_to_png, cleanup_beos},
    { "beos-bitmap", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR,
      create_beos_bitmap_surface, cairo_surface_write_to_png, cleanup_beos_bitmap},
    { "beos-bitmap", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR_ALPHA,
      create_beos_bitmap_surface, cairo_surface_write_to_png, cleanup_beos_bitmap},
#endif

#if CAIRO_HAS_DIRECTFB_SURFACE
    { "directfb", CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR,
      create_directfb_surface, cairo_surface_write_to_png, cleanup_directfb},
    { "directfb-bitmap", CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR_ALPHA,
      create_directfb_bitmap_surface, cairo_surface_write_to_png,cleanup_directfb},
#endif

    { NULL }
};

void
xasprintf (char **strp, const char *fmt, ...)
{
#ifdef HAVE_VASPRINTF
    va_list va;
    int ret;

    va_start (va, fmt);
    ret = vasprintf (strp, fmt, va);
    va_end (va);

    if (ret < 0) {
	cairo_test_log ("Out of memory\n");
	exit (1);
    }
#else /* !HAVE_VASNPRINTF */
#define BUF_SIZE 1024
    va_list va;
    char buffer[BUF_SIZE];
    int ret;

    va_start (va, fmt);
    ret = vsnprintf (buffer, sizeof(buffer), fmt, va);
    va_end (va);

    if (ret < 0) {
	CAIRO_BOILERPLATE_LOG ("Failure in vsnprintf\n");
	exit (1);
    }

    if (strlen (buffer) == sizeof(buffer) - 1) {
	CAIRO_BOILERPLATE_LOG ("Overflowed fixed buffer\n");
	exit (1);
    }

    *strp = strdup (buffer);
    if (!*strp) {
	CAIRO_BOILERPLATE_LOG ("Out of memory\n");
	exit (1);
    }
#endif /* !HAVE_VASNPRINTF */
}
