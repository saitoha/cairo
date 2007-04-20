/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © 2004,2006 Red Hat, Inc.
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

#if CAIRO_HAS_BEOS_SURFACE
#include "cairo-boilerplate-beos-private.h"
#endif
#if CAIRO_HAS_DIRECTFB_SURFACE
#include "cairo-boilerplate-directfb-private.h"
#endif
#if CAIRO_HAS_PDF_SURFACE
#include "cairo-boilerplate-pdf-private.h"
#endif
#if CAIRO_HAS_PS_SURFACE
#include "cairo-boilerplate-ps-private.h"
#endif
#if CAIRO_HAS_QUARTZ_SURFACE
#include "cairo-boilerplate-quartz-private.h"
#endif
#if CAIRO_HAS_XLIB_XRENDER_SURFACE
#include "cairo-boilerplate-xlib-private.h"
#endif

/* This is copied from cairoint.h. That makes it painful to keep in
 * sync, but the slim stuff makes cairoint.h "hard" to include when
 * not actually building the cairo library itself. Fortunately, since
 * we're checking all these values, we do have a safeguard for keeping
 * them in sync.
 */
typedef enum cairo_internal_surface_type {
    CAIRO_INTERNAL_SURFACE_TYPE_META = 0x1000,
    CAIRO_INTERNAL_SURFACE_TYPE_PAGINATED,
    CAIRO_INTERNAL_SURFACE_TYPE_ANALYSIS,
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED
} cairo_internal_surface_type_t;

const char *
cairo_boilerplate_content_name (cairo_content_t content)
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
_cairo_boilerplate_image_create_surface (const char			 *name,
					 cairo_content_t		  content,
					 int				  width,
					 int				  height,
					 cairo_boilerplate_mode_t	  mode,
					 void				**closure)
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
_cairo_boilerplate_test_fallback_create_surface (const char			 *name,
						 cairo_content_t		  content,
						 int				  width,
						 int				  height,
						 cairo_boilerplate_mode_t	  mode,
						 void				**closure)
{
    *closure = NULL;
    return _cairo_test_fallback_surface_create (content, width, height);
}

static cairo_surface_t *
_cairo_boilerplate_test_meta_create_surface (const char			 *name,
					     cairo_content_t		  content,
					     int			  width,
					     int			  height,
					     cairo_boilerplate_mode_t	  mode,
					     void			**closure)
{
    *closure = NULL;
    return _cairo_test_meta_surface_create (content, width, height);
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
_cairo_boilerplate_test_paginated_create_surface (const char			 *name,
						  cairo_content_t		  content,
						  int				  width,
						  int				  height,
						  cairo_boilerplate_mode_t	  mode,
						  void				**closure)
{
    test_paginated_closure_t *tpc;
    cairo_surface_t *surface;

    *closure = tpc = xmalloc (sizeof (test_paginated_closure_t));

    tpc->content = content;
    tpc->width = width;
    tpc->height = height;
    tpc->stride = width * 4;

    tpc->data = xcalloc (tpc->stride * height, 1);

    surface = _cairo_test_paginated_surface_create_for_data (tpc->data,
						       tpc->content,
						       tpc->width,
						       tpc->height,
						       tpc->stride);

    cairo_boilerplate_surface_set_user_data (surface,
					     &test_paginated_closure_key,
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
_cairo_boilerplate_test_paginated_surface_write_to_png (cairo_surface_t *surface,
			     const char	     *filename)
{
    cairo_surface_t *image;
    cairo_format_t format;
    test_paginated_closure_t *tpc;
    cairo_status_t status;

    /* show page first.  the automatic show_page is too late for us */
    /* XXX use cairo_surface_show_page() when that's added */
    cairo_t *cr = cairo_create (surface);
    cairo_show_page (cr);
    cairo_destroy (cr);

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

    status = cairo_surface_write_to_png (image, filename);
    if (status) {
	CAIRO_BOILERPLATE_LOG ("Error writing %s: %s. Exiting\n",
			       filename,
			       cairo_status_to_string (status));
	exit (1);
    }

    cairo_surface_destroy (image);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_boilerplate_test_paginated_cleanup (void *closure)
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
_cairo_boilerplate_glitz_glx_create_surface (glitz_format_name_t	 formatname,
					     int			 width,
					     int			 height,
					     glitz_glx_target_closure_t	*closure)
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
_cairo_boilerplate_cairo_glitz_glx_create_surface (const char			 *name,
						   cairo_content_t 		  content,
						   int				  width,
						   int				  height,
						   cairo_boilerplate_mode_t	  mode,
						   void				**closure)
{
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
	glitz_surface = _cairo_boilerplate_glitz_glx_create_surface (GLITZ_STANDARD_RGB24, width, height, gxtc);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = _cairo_boilerplate_glitz_glx_create_surface (GLITZ_STANDARD_ARGB32, width, height, gxtc);
	break;
    case CAIRO_CONTENT_ALPHA:
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
_cairo_boilerplate_cairo_glitz_glx_cleanup (void *closure)
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
_cairo_boilerplate_glitz_agl_create_surface (glitz_format_name_t	 formatname,
					     int			 width,
					     int			 height,
					     glitz_agl_target_closure_t	*closure)
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
_cairo_boilerplate_cairo_glitz_agl_create_surface (const char			 *name,
						   cairo_content_t 		  content,
						   int				  width,
						   int				  height,
						   cairo_boilerplate_mode_t	  mode,
						   void				**closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface;
    glitz_agl_target_closure_t *aglc;

    glitz_agl_init ();

    *closure = aglc = xmalloc (sizeof (glitz_agl_target_closure_t));

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	glitz_surface = _cairo_boilerplate_glitz_agl_create_surface (GLITZ_STANDARD_RGB24, width, height, NULL);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = _cairo_boilerplate_glitz_agl_create_surface (GLITZ_STANDARD_ARGB32, width, height, NULL);
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
_cairo_boilerplate_cairo_glitz_agl_cleanup (void *closure)
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
_cairo_boilerplate_glitz_wgl_create_surface (glitz_format_name_t	 formatname,
					     int			 width,
					     int			 height,
					     glitz_wgl_target_closure_t	*closure)
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
_cairo_boilerplate_cairo_glitz_wgl_create_surface (const char			 *name,
						   cairo_content_t		  content,
						   int				  width,
						   int				  height,
						   cairo_boilerplate_mode_t	  mode,
						   void				**closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface;
    glitz_wgl_target_closure_t *wglc;

    glitz_wgl_init (NULL);

    *closure = wglc = xmalloc (sizeof (glitz_wgl_target_closure_t));

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	glitz_surface = _cairo_boilerplate_glitz_wgl_create_surface (GLITZ_STANDARD_RGB24, width, height, NULL);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = _cairo_boilerplate_glitz_wgl_create_surface (GLITZ_STANDARD_ARGB32, width, height, NULL);
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
_cairo_boilerplate_cairo_glitz_wgl_cleanup (void *closure)
{
    free (closure);
    glitz_wgl_fini ();
}

#endif /* CAIRO_CAN_TEST_GLITZ_WGL_SURFACE */

#endif /* CAIRO_HAS_GLITZ_SURFACE */

/* Testing the win32 surface isn't interesting, since for
 * ARGB images it just chains to the image backend
 */
#if CAIRO_HAS_WIN32_SURFACE
#include "cairo-win32.h"
static cairo_surface_t *
create_win32_surface (const char		 *name,
		      cairo_content_t		  content,
		      int			  width,
		      int			  height,
		      cairo_boilerplate_mode_t	  mode,
		      void			**closure)
{
    cairo_format_t format;

    if (content == CAIRO_CONTENT_COLOR)
        format = CAIRO_FORMAT_RGB24;
    else if (content == CAIRO_CONTENT_COLOR_ALPHA)
        format = CAIRO_FORMAT_ARGB32;
    else
        return NULL;

    *closure = NULL;
    return cairo_win32_surface_create_with_dib (format, width, height);
}

static void
cleanup_win32 (void *closure)
{
}
#endif

#if CAIRO_HAS_XCB_SURFACE
#include "cairo-xcb-xrender.h"
#include <xcb/xcb_renderutil.h>
typedef struct _xcb_target_closure
{
    xcb_connection_t *c;
    xcb_pixmap_t pixmap;
} xcb_target_closure_t;

static void
_cairo_boilerplate_xcb_synchronize (void *closure)
{
    xcb_target_closure_t *xtc = closure;
    free (xcb_get_image_reply (xtc->c,
		xcb_get_image (xtc->c, XCB_IMAGE_FORMAT_Z_PIXMAP,
		    xtc->pixmap, 0, 0, 1, 1, /* AllPlanes */ ~0UL),
		0));
}

static cairo_surface_t *
_cairo_boilerplate_xcb_create_surface (const char		 *name,
				       cairo_content_t		  content,
				       int			  width,
				       int			  height,
				       cairo_boilerplate_mode_t	  mode,
				       void			**closure)
{
    xcb_screen_t *root;
    xcb_target_closure_t *xtc;
    cairo_surface_t *surface;
    xcb_connection_t *c;
    xcb_render_pictforminfo_t *render_format;
    xcb_pict_standard_t format;

    *closure = xtc = xmalloc (sizeof (xcb_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->c = c = xcb_connect(NULL,NULL);
    if (xcb_connection_has_error(c)) {
	CAIRO_BOILERPLATE_LOG ("Failed to connect to X server through XCB\n");
	return NULL;
    }

    root = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    xtc->pixmap = xcb_generate_id (c);
    xcb_create_pixmap (c, 32, xtc->pixmap, root->root,
			 width, height);

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	format = XCB_PICT_STANDARD_RGB_24;
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = XCB_PICT_STANDARD_ARGB_32;
	break;
    case CAIRO_CONTENT_ALPHA:  /* would be XCB_PICT_STANDARD_A_8 */
    default:
	CAIRO_BOILERPLATE_LOG ("Invalid content for XCB test: %d\n", content);
	return NULL;
    }

    render_format = xcb_render_util_find_standard_format (xcb_render_util_query_formats (c), format);
    if (render_format->id == 0)
	return NULL;
    surface = cairo_xcb_surface_create_with_xrender_format (c, xtc->pixmap, root,
							    render_format,
							    width, height);

    return surface;
}

static void
_cairo_boilerplate_xcb_cleanup (void *closure)
{
    xcb_target_closure_t *xtc = closure;

    xcb_free_pixmap (xtc->c, xtc->pixmap);
    xcb_disconnect (xtc->c);
    free (xtc);
}
#endif

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
_cairo_boilerplate_svg_create_surface (const char		 *name,
				       cairo_content_t		  content,
				       int			  width,
				       int			  height,
				       cairo_boilerplate_mode_t	  mode,
				       void			**closure)
{
    svg_target_closure_t *ptc;
    cairo_surface_t *surface;

    *closure = ptc = xmalloc (sizeof (svg_target_closure_t));

    ptc->width = width;
    ptc->height = height;

    xasprintf (&ptc->filename, "%s-svg-%s-out.svg",
	       name, cairo_boilerplate_content_name (content));

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

    cairo_boilerplate_surface_set_user_data (surface,
					     &svg_closure_key,
					     ptc, NULL);

    return surface;
}

static cairo_status_t
_cairo_boilerplate_svg_surface_write_to_png (cairo_surface_t *surface, const char *filename)
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
_cairo_boilerplate_svg_cleanup (void *closure)
{
    svg_target_closure_t *ptc = closure;
    if (ptc->target)
	cairo_surface_destroy (ptc->target);
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE */

static cairo_boilerplate_target_t targets[] =
{
    /* I'm uncompromising about leaving the image backend as 0
     * for tolerance. There shouldn't ever be anything that is out of
     * our control here. */
    { "image", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_image_create_surface,
      cairo_surface_write_to_png },
    { "image", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_image_create_surface,
      cairo_surface_write_to_png },
#ifdef CAIRO_HAS_TEST_SURFACES
    { "test-fallback", CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
      CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_test_fallback_create_surface,
      cairo_surface_write_to_png },
    { "test-fallback", CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
      CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_test_fallback_create_surface,
      cairo_surface_write_to_png },
    { "test-meta", CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
      CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_test_meta_create_surface,
      cairo_surface_write_to_png },
    { "test-meta", CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
      CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_test_meta_create_surface,
      cairo_surface_write_to_png },
    { "test-paginated", CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,
      CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_test_paginated_create_surface,
      _cairo_boilerplate_test_paginated_surface_write_to_png,
      _cairo_boilerplate_test_paginated_cleanup },
    { "test-paginated", CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,
      CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_test_paginated_create_surface,
      _cairo_boilerplate_test_paginated_surface_write_to_png,
      _cairo_boilerplate_test_paginated_cleanup },
#endif
#ifdef CAIRO_HAS_GLITZ_SURFACE
#if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
    { "glitz-glx", CAIRO_SURFACE_TYPE_GLITZ,CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_cairo_glitz_glx_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_cairo_glitz_glx_cleanup },
    { "glitz-glx", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_cairo_glitz_glx_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_cairo_glitz_glx_cleanup },
#endif
#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
    { "glitz-agl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_cairo_glitz_agl_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_cairo_glitz_agl_cleanup },
    { "glitz-agl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_cairo_glitz_agl_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_cairo_glitz_agl_cleanup },
#endif
#if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
    { "glitz-wgl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_cairo_glitz_wgl_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_cairo_glitz_wgl_cleanup },
    { "glitz-wgl", CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_cairo_glitz_wgl_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_cairo_glitz_wgl_cleanup },
#endif
#endif /* CAIRO_HAS_GLITZ_SURFACE */
#if CAIRO_HAS_QUARTZ_SURFACE
    { "quartz", CAIRO_SURFACE_TYPE_QUARTZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_quartz_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_quartz_cleanup },
    { "quartz", CAIRO_SURFACE_TYPE_QUARTZ, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_quartz_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_quartz_cleanup },
#endif
#if CAIRO_HAS_WIN32_SURFACE
    { "win32", CAIRO_SURFACE_TYPE_WIN32, CAIRO_CONTENT_COLOR, 0,
      create_win32_surface,
      cairo_surface_write_to_png,
      cleanup_win32 },
    { "win32", CAIRO_SURFACE_TYPE_WIN32, CAIRO_CONTENT_COLOR_ALPHA, 0,
      create_win32_surface,
      cairo_surface_write_to_png,
      cleanup_win32 },
#endif
#if CAIRO_HAS_XCB_SURFACE
    /* Acceleration architectures may make the results differ by a
     * bit, so we set the error tolerance to 1. */
    { "xcb", CAIRO_SURFACE_TYPE_XCB, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_xcb_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xcb_cleanup,
      _cairo_boilerplate_xcb_synchronize},
#endif
#if CAIRO_HAS_XLIB_XRENDER_SURFACE
    /* Acceleration architectures may make the results differ by a
     * bit, so we set the error tolerance to 1. */
    { "xlib", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_xlib_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xlib_cleanup,
      _cairo_boilerplate_xlib_synchronize},
    { "xlib", CAIRO_SURFACE_TYPE_XLIB, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_xlib_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_xlib_cleanup,
      _cairo_boilerplate_xlib_synchronize},
#endif
#if CAIRO_HAS_PS_SURFACE
    { "ps", CAIRO_SURFACE_TYPE_PS,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED, 0,
      _cairo_boilerplate_ps_create_surface,
      _cairo_boilerplate_ps_surface_write_to_png,
      _cairo_boilerplate_ps_cleanup,
      NULL, TRUE },

    /* XXX: We expect type image here only due to a limitation in
     * the current PS/meta-surface code. A PS surface is
     * "naturally" COLOR_ALPHA, so the COLOR-only variant goes
     * through create_similar in _cairo_boilerplate_ps_create_surface which results
     * in the similar surface being used as a source. We do not yet
     * have source support for PS/meta-surfaces, so the
     * create_similar path for all paginated surfaces currently
     * returns an image surface.*/
    { "ps", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_ps_create_surface,
      _cairo_boilerplate_ps_surface_write_to_png,
      _cairo_boilerplate_ps_cleanup,
      NULL, TRUE },
#endif
#if CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE
    { "pdf", CAIRO_SURFACE_TYPE_PDF,
      CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED, 0,
      _cairo_boilerplate_pdf_create_surface,
      _cairo_boilerplate_pdf_surface_write_to_png,
      _cairo_boilerplate_pdf_cleanup,
      NULL, TRUE },

    /* XXX: We expect type image here only due to a limitation in
     * the current PDF/meta-surface code. A PDF surface is
     * "naturally" COLOR_ALPHA, so the COLOR-only variant goes
     * through create_similar in _cairo_boilerplate_pdf_create_surface which results
     * in the similar surface being used as a source. We do not yet
     * have source support for PDF/meta-surfaces, so the
     * create_similar path for all paginated surfaces currently
     * returns an image surface.*/
    { "pdf", CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_pdf_create_surface,
      _cairo_boilerplate_pdf_surface_write_to_png,
      _cairo_boilerplate_pdf_cleanup,
      NULL, TRUE },
#endif
#if CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE
    /* It seems we should be able to round-trip SVG content perfrectly
     * through librsvg and cairo, but for some mysterious reason, some
     * systems get an error of 1 for some pixels on some of the text
     * tests. XXX: I'd still like to chase these down at some point.
     * For now just set the svg error tolerance to 1. */
    { "svg", CAIRO_SURFACE_TYPE_SVG, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_svg_create_surface,
      _cairo_boilerplate_svg_surface_write_to_png,
      _cairo_boilerplate_svg_cleanup,
      NULL, TRUE },
    { "svg", CAIRO_INTERNAL_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_svg_create_surface,
      _cairo_boilerplate_svg_surface_write_to_png,
      _cairo_boilerplate_svg_cleanup,
      NULL, TRUE },
#endif
#if CAIRO_HAS_BEOS_SURFACE
    /* BeOS sometimes produces a slightly different image. Perhaps this
     * is related to the fact that it doesn't use premultiplied alpha...
     * Just ignore the small difference. */
    { "beos", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_beos_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_beos_cleanup},
    { "beos-bitmap", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR, 1,
      _cairo_boilerplate_beos_create_surface_for_bitmap,
      cairo_surface_write_to_png,
      _cairo_boilerplate_beos_cleanup_bitmap},
    { "beos-bitmap", CAIRO_SURFACE_TYPE_BEOS, CAIRO_CONTENT_COLOR_ALPHA, 1,
      _cairo_boilerplate_beos_create_surface_for_bitmap,
      cairo_surface_write_to_png,
      _cairo_boilerplate_beos_cleanup_bitmap},
#endif


#if CAIRO_HAS_DIRECTFB_SURFACE
    { "directfb", CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR, 0,
      _cairo_boilerplate_directfb_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_directfb_cleanup},
    { "directfb-bitmap", CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR_ALPHA, 0,
      _cairo_boilerplate_directfb_create_surface,
      cairo_surface_write_to_png,
      _cairo_boilerplate_directfb_cleanup},
#endif
};

cairo_boilerplate_target_t **
cairo_boilerplate_get_targets (int *pnum_targets, cairo_bool_t *plimited_targets)
{
    size_t i, num_targets;
    cairo_bool_t limited_targets = FALSE;
    const char *tname;
    cairo_boilerplate_target_t **targets_to_test;

    if ((tname = getenv ("CAIRO_TEST_TARGET")) != NULL && *tname) {

	limited_targets = TRUE;

	num_targets = 0;
	targets_to_test = NULL;

	while (*tname) {
	    int found = 0;
	    const char *end = strpbrk (tname, " \t\r\n;:,");
	    if (!end)
	        end = tname + strlen (tname);

	    if (end == tname) {
		tname = end + 1;
		continue;
	    }

	    for (i = 0; i < sizeof (targets) / sizeof (targets[0]); i++) {
		if (0 == strncmp (targets[i].name, tname, end - tname) &&
		    !isalnum (targets[i].name[end - tname])) {
		    /* realloc isn't exactly the best thing here, but meh. */
		    targets_to_test = realloc (targets_to_test, sizeof(cairo_boilerplate_target_t *) * (num_targets+1));
		    targets_to_test[num_targets++] = &targets[i];
		    found = 1;
		}
	    }

	    if (!found) {
		fprintf (stderr, "Cannot find target '%.*s'\n", (int)(end - tname), tname);
		exit(-1);
	    }

	    if (*end)
	      end++;
	    tname = end;
	}
    } else {
	num_targets = sizeof (targets) / sizeof (targets[0]);
	targets_to_test = malloc (sizeof(cairo_boilerplate_target_t*) * num_targets);
	for (i = 0; i < num_targets; i++) {
	    targets_to_test[i] = &targets[i];
	}
    }

    if (pnum_targets)
	*pnum_targets = num_targets;

    if (plimited_targets)
	*plimited_targets = limited_targets;

    return targets_to_test;
}

void
cairo_boilerplate_free_targets (cairo_boilerplate_target_t **targets)
{
    free (targets);
}

void
cairo_boilerplate_surface_set_user_data (cairo_surface_t		*surface,
					 const cairo_user_data_key_t	*key,
					 void				*user_data,
					 cairo_destroy_func_t		 destroy)
{
    cairo_status_t status;

    status = cairo_surface_set_user_data (surface,
					  key, user_data,
					  destroy);
    if (status) {
	CAIRO_BOILERPLATE_LOG ("Error: %s. Exiting\n",
			       cairo_status_to_string (status));
	exit (1);
    }
}

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
