/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2006 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-pdf.h"
#include "cairo-font-subset-private.h"
#include "cairo-ft-private.h"
#include "cairo-paginated-surface-private.h"
#include "cairo-path-fixed-private.h"

#include <time.h>
#include <zlib.h>

/* Issues:
 *
 * - Why doesn't pages inherit /alpha%d GS dictionaries from the Pages
 *   object?
 *
 * - We embed an image in the stream each time it's composited.  We
 *   could add generation counters to surfaces and remember the stream
 *   ID for a particular generation for a particular surface.
 *
 * - Clipping: must be able to reset clipping
 *
 * - Images of other formats than 8 bit RGBA.
 *
 * - Backend specific meta data.
 *
 * - Surface patterns.
 *
 * - Alpha channels in gradients.
 *
 * - Should/does cairo support drawing into a scratch surface and then
 *   using that as a fill pattern?  For this backend, that would involve
 *   using a tiling pattern (4.6.2).  How do you create such a scratch
 *   surface?  cairo_surface_create_similar() ?
 *
 * - What if you create a similiar surface and does show_page and then
 *   does show_surface on another surface?
 *
 * - Output TM so page scales to the right size - PDF default user
 *   space has 1 unit = 1 / 72 inch.
 *
 * - Add test case for RGBA images.
 *
 * - Add test case for RGBA gradients.
 *
 * - Coordinate space for create_similar() args?
 *
 * - Investigate /Matrix entry in content stream dicts for pages
 *   instead of outputting the cm operator in every page.
 */

typedef struct cairo_pdf_object cairo_pdf_object_t;
typedef struct cairo_pdf_resource cairo_pdf_resource_t;
typedef struct cairo_pdf_document cairo_pdf_document_t;
typedef struct cairo_pdf_surface cairo_pdf_surface_t;

struct cairo_pdf_object {
    long offset;
};

struct cairo_pdf_resource {
    unsigned int id;
};

struct cairo_pdf_document {
    cairo_output_stream_t *output_stream;
    unsigned long ref_count;
    cairo_surface_t *owner;
    cairo_bool_t finished;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    cairo_pdf_resource_t next_available_resource;
    cairo_pdf_resource_t pages_resource;

    struct {
	cairo_bool_t active;
	cairo_pdf_resource_t self;
	cairo_pdf_resource_t length;
	long start_offset;
    } current_stream;

    cairo_array_t objects;
    cairo_array_t pages;

    cairo_array_t fonts;
};

struct cairo_pdf_surface {
    cairo_surface_t base;

    double width;
    double height;

    cairo_pdf_document_t *document;

    cairo_array_t patterns;
    cairo_array_t xobjects;
    cairo_array_t streams;
    cairo_array_t alphas;
    cairo_array_t fonts;
    cairo_bool_t has_clip;

    cairo_paginated_mode_t paginated_mode;
};

#define DEFAULT_DPI 300

static cairo_pdf_document_t *
_cairo_pdf_document_create (cairo_output_stream_t	*stream,
			    double			width,
			    double			height);

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document);

static cairo_status_t
_cairo_pdf_document_finish (cairo_pdf_document_t *document);

static cairo_pdf_document_t *
_cairo_pdf_document_reference (cairo_pdf_document_t *document);

static cairo_pdf_resource_t
_cairo_pdf_document_new_object (cairo_pdf_document_t *document);

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t *document,
			      cairo_pdf_surface_t *surface);

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface);

static cairo_pdf_resource_t
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*fmt,
				 ...) CAIRO_PRINTF_FORMAT(2, 3);
static void
_cairo_pdf_document_close_stream (cairo_pdf_document_t	*document);

static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width,
					double			height);
static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_resource_t	 stream);

static const cairo_surface_backend_t cairo_pdf_surface_backend;
static const cairo_paginated_surface_backend_t cairo_pdf_surface_paginated_backend;

static cairo_pdf_resource_t
_cairo_pdf_document_new_object (cairo_pdf_document_t *document)
{
    cairo_pdf_resource_t resource;
    cairo_status_t status;
    cairo_pdf_object_t object;

    object.offset = _cairo_output_stream_get_position (document->output_stream);

    status = _cairo_array_append (&document->objects, &object);
    if (status) {
	resource.id = 0;
	return resource;
    }

    resource = document->next_available_resource;
    document->next_available_resource.id++;

    return resource;
}

static void
_cairo_pdf_document_update_object (cairo_pdf_document_t *document,
				   cairo_pdf_resource_t	 resource)
{
    cairo_pdf_object_t *object;

    object = _cairo_array_index (&document->objects, resource.id - 1);
    object->offset = _cairo_output_stream_get_position (document->output_stream);
}

static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_resource_t	 stream)
{
    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->streams, &stream);
}

static void
_cairo_pdf_surface_add_pattern (cairo_pdf_surface_t	*surface,
				cairo_pdf_resource_t	 pattern)
{
    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->patterns, &pattern);
}

static cairo_pdf_resource_t
_cairo_pdf_surface_add_alpha (cairo_pdf_surface_t *surface, double alpha)
{
    cairo_pdf_resource_t resource;
    int num_alphas, i;
    double other;

    num_alphas = _cairo_array_num_elements (&surface->alphas);
    for (i = 0; i < num_alphas; i++) {
	_cairo_array_copy_element (&surface->alphas, i, &other);
	if (alpha == other) {
	    resource.id  = i;
	    return resource;
	}
    }

    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->alphas, &alpha);

    resource.id = _cairo_array_num_elements (&surface->alphas) - 1;
    return resource;
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			 width,
					       double			 height)
{
    cairo_pdf_document_t *document;
    cairo_surface_t *target;

    document = _cairo_pdf_document_create (stream, width, height);
    if (document == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    target = _cairo_pdf_surface_create_for_document (document, width, height);

    document->owner = target;
    _cairo_pdf_document_destroy (document);

    return _cairo_paginated_surface_create (target,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height,
					    &cairo_pdf_surface_paginated_backend);
}

/**
 * cairo_pdf_surface_create_for_stream:
 * @write: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PDF surface of the specified size in points to be written
 * incrementally to the stream represented by @write and @closure.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 */
cairo_surface_t *
cairo_pdf_surface_create_for_stream (cairo_write_func_t		 write,
				     void			*closure,
				     double			 width_in_points,
				     double			 height_in_points)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write, NULL, closure);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_pdf_surface_create_for_stream_internal (stream,
							  width_in_points,
							  height_in_points);
}

/**
 * cairo_pdf_surface_create:
 * @filename: a filename for the PDF output (must be writable)
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PDF surface of the specified size in points to be written
 * to @filename.
 * 
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_pdf_surface_create (const char		*filename,
			  double		 width_in_points,
			  double		 height_in_points)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_filename (filename);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_pdf_surface_create_for_stream_internal (stream,
							  width_in_points,
							  height_in_points);
}

static cairo_bool_t
_cairo_surface_is_pdf (cairo_surface_t *surface)
{
    return surface->backend == &cairo_pdf_surface_backend;
}

/* If the abstract_surface is a paginated surface, and that paginated
 * surface's target is a pdf_surface, then set pdf_surface to that
 * target. Otherwise return CAIRO_STATUS_SURFACE_TYPE_MISMATCH.
 */
static cairo_status_t
_extract_pdf_surface (cairo_surface_t		 *surface,
		      cairo_pdf_surface_t	**pdf_surface)
{
    cairo_surface_t *target;

    if (! _cairo_surface_is_paginated (surface))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_pdf (target))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    *pdf_surface = (cairo_pdf_surface_t *) target;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pdf_surface_set_dpi:
 * @surface: a PDF cairo_surface_t
 * @x_dpi: horizontal dpi
 * @y_dpi: vertical dpi
 * 
 * Set the horizontal and vertical resolution for image fallbacks.
 * When the pdf backend needs to fall back to image overlays, it will
 * use this resolution. These DPI values are not used for any other
 * purpose, (in particular, they do not have any bearing on the size
 * passed to cairo_pdf_surface_create() nor on the CTM).
 **/
void
cairo_pdf_surface_set_dpi (cairo_surface_t	*surface,
			   double		x_dpi,
			   double		y_dpi)
{
    cairo_pdf_surface_t *pdf_surface;
    cairo_status_t status;

    status = _extract_pdf_surface (surface, &pdf_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    pdf_surface->document->x_dpi = x_dpi;    
    pdf_surface->document->y_dpi = y_dpi;    
}

/**
 * cairo_pdf_surface_set_size:
 * @surface: a PDF cairo_surface_t
 * @width_in_points: new surface width, in points (1 point == 1/72.0 inch)
 * @height_in_points: new surface height, in points (1 point == 1/72.0 inch)
 * 
 * Changes the size of a PDF surface for the current (and
 * subsequent) pages.
 *
 * This function should only be called before any drawing operations
 * have been performed on the current page. The simplest way to do
 * this is to call this function immediately after creating the
 * surface or immediately after completing a page with either
 * cairo_show_page() or cairo_copy_page().
 **/
void
cairo_pdf_surface_set_size (cairo_surface_t	*surface,
			    double		 width_in_points,
			    double		 height_in_points)
{
    cairo_pdf_surface_t *pdf_surface;
    cairo_status_t status;

    status = _extract_pdf_surface (surface, &pdf_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    pdf_surface->width = width_in_points;
    pdf_surface->height = height_in_points;
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width,
					double			height)
{
    cairo_pdf_surface_t *surface;

    surface = malloc (sizeof (cairo_pdf_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_pdf_surface_backend);

    surface->width = width;
    surface->height = height;

    surface->document = _cairo_pdf_document_reference (document);
    _cairo_array_init (&surface->streams, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->patterns, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->xobjects, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->alphas, sizeof (double));
    _cairo_array_init (&surface->fonts, sizeof (cairo_pdf_resource_t));
    surface->has_clip = FALSE;

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;

    return &surface->base;
}

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface)
{
    _cairo_array_truncate (&surface->streams, 0);
    _cairo_array_truncate (&surface->patterns, 0);
    _cairo_array_truncate (&surface->xobjects, 0);
    _cairo_array_truncate (&surface->alphas, 0);
    _cairo_array_truncate (&surface->fonts, 0);
}

static cairo_surface_t *
_cairo_pdf_surface_create_similar (void		       *abstract_src,
				   cairo_content_t	content,
				   int			width,
				   int			height)
{
    cairo_format_t format = _cairo_format_from_content (content);

    /* Just return an image for now, until PDF surface can be used
     * as source. */
    return cairo_image_surface_create (format, width, height);
}

static cairo_pdf_resource_t
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*fmt,
				 ...)
{
    cairo_output_stream_t *output_stream = document->output_stream;
    va_list ap;

    document->current_stream.active = TRUE;
    document->current_stream.self = _cairo_pdf_document_new_object (document);
    document->current_stream.length = _cairo_pdf_document_new_object (document);

    _cairo_output_stream_printf (output_stream,
				 "%d 0 obj\r\n"
				 "<< /Length %d 0 R\r\n",
				 document->current_stream.self.id,
				 document->current_stream.length.id);

    if (fmt != NULL) {
	va_start (ap, fmt);
	_cairo_output_stream_vprintf (output_stream, fmt, ap);
	va_end (ap);
    }

    _cairo_output_stream_printf (output_stream,
				 ">>\r\n"
				 "stream\r\n");

    document->current_stream.start_offset = _cairo_output_stream_get_position (output_stream);

    return document->current_stream.self;
}

static void
_cairo_pdf_document_close_stream (cairo_pdf_document_t	*document)
{
    cairo_output_stream_t *output_stream = document->output_stream;
    long length;

    if (! document->current_stream.active)
	return;

    length = _cairo_output_stream_get_position (output_stream) -
	document->current_stream.start_offset;
    _cairo_output_stream_printf (output_stream,
				 "endstream\r\n"
				 "endobj\r\n");

    _cairo_pdf_document_update_object (document,
				       document->current_stream.length);
    _cairo_output_stream_printf (output_stream,
				 "%d 0 obj\r\n"
				 "   %ld\r\n"
				 "endobj\r\n",
				 document->current_stream.length.id,
				 length);

    document->current_stream.active = FALSE;
}

static cairo_status_t
_cairo_pdf_surface_finish (void *abstract_surface)
{
    cairo_status_t status;
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    _cairo_pdf_document_close_stream (document);

    if (document->owner == &surface->base)
	status = _cairo_pdf_document_finish (document);
    else
	status = CAIRO_STATUS_SUCCESS;

    _cairo_pdf_document_destroy (document);

    _cairo_array_fini (&surface->streams);
    _cairo_array_fini (&surface->patterns);
    _cairo_array_fini (&surface->xobjects);
    _cairo_array_fini (&surface->alphas);
    _cairo_array_fini (&surface->fonts);

    return status;
}

static void 
_cairo_pdf_surface_pause_content_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_document_t *document = surface->document;

    _cairo_pdf_document_close_stream (document);
}

static void
_cairo_pdf_surface_resume_content_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_pdf_resource_t stream;

    stream = _cairo_pdf_document_open_stream (document,
					      "   /Type /XObject\r\n"
					      "   /Subtype /Form\r\n"
					      "   /BBox [ 0 0 %f %f ]\r\n",
					      surface->width,
					      surface->height);

    _cairo_pdf_surface_add_stream (surface, stream);
}

static cairo_int_status_t
_cairo_pdf_surface_start_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t stream;

    stream = _cairo_pdf_document_open_stream (document,
					      "   /Type /XObject\r\n"
					      "   /Subtype /Form\r\n"
					      "   /BBox [ 0 0 %f %f ]\r\n",
					      surface->width,
					      surface->height);

    _cairo_pdf_surface_add_stream (surface, stream);

    _cairo_output_stream_printf (output,
				 "1 0 0 -1 0 %f cm\r\n",
				 surface->height);

    return CAIRO_STATUS_SUCCESS;
}

static void *
compress_dup (const void *data, unsigned long data_size,
	      unsigned long *compressed_size)
{
    void *compressed;

    /* Bound calculation taken from zlib. */
    *compressed_size = data_size + (data_size >> 12) + (data_size >> 14) + 11;
    compressed = malloc (*compressed_size);
    if (compressed == NULL)
	return NULL;

    compress (compressed, compressed_size, data, data_size);

    return compressed;
}

/* Emit alpha channel from the image into the given data, providing
 * and id that can be used to reference the resulting SMask object.
 *
 * In the case that the alpha channel happens to be all opaque, then
 * no SMask object will be emitted and *id_ret will be set to 0.
 */
static cairo_status_t
emit_smask (cairo_pdf_document_t	*document,
	    cairo_image_surface_t	*image,
	    cairo_pdf_resource_t	*stream_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_output_stream_t *output = document->output_stream;
    char *alpha, *alpha_compressed;
    unsigned long alpha_size, alpha_compressed_size;
    pixman_bits_t *pixel;
    int i, x, y;
    cairo_bool_t opaque;
    uint8_t a;

    /* This is the only image format we support, which simplfies things. */
    assert (image->format == CAIRO_FORMAT_ARGB32);

    alpha_size = image->height * image->width;
    alpha = malloc (alpha_size);
    if (alpha == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP;
    }

    opaque = TRUE;
    i = 0;
    for (y = 0; y < image->height; y++) {
	pixel = (pixman_bits_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    a = (*pixel & 0xff000000) >> 24;
	    alpha[i++] = a;
	    if (a != 0xff)
		opaque = FALSE;
	}
    }

    /* Bail out without emitting smask if it's all opaque. */
    if (opaque) {
	stream_ret->id = 0;
	goto CLEANUP_ALPHA;
    }

    alpha_compressed = compress_dup (alpha, alpha_size, &alpha_compressed_size);
    if (alpha_compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_ALPHA;
	
    }

    *stream_ret = _cairo_pdf_document_open_stream (document,
						   "   /Type /XObject\r\n"
						   "   /Subtype /Image\r\n"
						   "   /Width %d\r\n"
						   "   /Height %d\r\n"
						   "   /ColorSpace /DeviceGray\r\n"
						   "   /BitsPerComponent 8\r\n"
						   "   /Filter /FlateDecode\r\n",
						   image->width, image->height);
    _cairo_output_stream_write (output, alpha_compressed, alpha_compressed_size);
    _cairo_output_stream_printf (output, "\r\n");
    _cairo_pdf_document_close_stream (document);

    free (alpha_compressed);
 CLEANUP_ALPHA:
    free (alpha);
 CLEANUP:
    return status;
}


/* Emit image data into the given document, providing an id that can
 * be used to reference the data in id_ret. */
static cairo_status_t
emit_image (cairo_pdf_document_t	*document,
	    cairo_image_surface_t	*image,
	    cairo_pdf_resource_t	*image_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_output_stream_t *output = document->output_stream;
    char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;
    pixman_bits_t *pixel;
    int i, x, y;
    cairo_pdf_resource_t smask;
    cairo_bool_t need_smask;

    /* XXX: Need to rewrite this as a pdf_surface function with
     * pause/resume of content_stream, (currently the only caller does
     * the pause/resume already, but that is expected to change in the
     * future). */

    /* These are the only image formats we currently support, (which
     * makes things a lot simpler here). This is enforeced through
     * _analyze_operation which only accept source surfaces of
     * CONTENT_COLOR or CONTENT_COLOR_ALPHA. 
     */
    assert (image->format == CAIRO_FORMAT_RGB24 || image->format == CAIRO_FORMAT_ARGB32);

    rgb_size = image->height * image->width * 3;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP;
    }

    i = 0;
    for (y = 0; y < image->height; y++) {
	pixel = (pixman_bits_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    /* XXX: We're un-premultiplying alpha here. My reading of the PDF
	     * specification suggests that we should be able to avoid having
	     * to do this by filling in the SMask's Matte dictionary
	     * appropriately, but my attempts to do that so far have
	     * failed. */
	    if (image->format == CAIRO_FORMAT_ARGB32) {
		uint8_t a;
		a = (*pixel & 0xff000000) >> 24;
		if (a == 0) {
		    rgb[i++] = 0;
		    rgb[i++] = 0;
		    rgb[i++] = 0;
		} else {
		    rgb[i++] = (((*pixel & 0xff0000) >> 16) * 255 + a / 2) / a;
		    rgb[i++] = (((*pixel & 0x00ff00) >>  8) * 255 + a / 2) / a;
		    rgb[i++] = (((*pixel & 0x0000ff) >>  0) * 255 + a / 2) / a;
		}
	    } else {
		rgb[i++] = (*pixel & 0x00ff0000) >> 16;
		rgb[i++] = (*pixel & 0x0000ff00) >>  8;
		rgb[i++] = (*pixel & 0x000000ff) >>  0;
	    }
	}
    }

    compressed = compress_dup (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_RGB;
    }

    need_smask = FALSE;
    if (image->format == CAIRO_FORMAT_ARGB32) {
	status = emit_smask (document, image, &smask);
	if (status)
	    goto CLEANUP_COMPRESSED;

	if (smask.id)
	    need_smask = TRUE;
    }

#define IMAGE_DICTIONARY	"   /Type /XObject\r\n"		\
				"   /Subtype /Image\r\n"	\
				"   /Width %d\r\n"		\
				"   /Height %d\r\n"		\
				"   /ColorSpace /DeviceRGB\r\n"	\
				"   /BitsPerComponent 8\r\n"	\
				"   /Filter /FlateDecode\r\n"


    if (need_smask)
	*image_ret = _cairo_pdf_document_open_stream (document,
						      IMAGE_DICTIONARY
						      "   /SMask %d 0 R\r\n",
						      image->width, image->height,
						      smask.id);
    else
	*image_ret = _cairo_pdf_document_open_stream (document,
						      IMAGE_DICTIONARY,
						      image->width, image->height);

#undef IMAGE_DICTIONARY

    _cairo_output_stream_write (output, compressed, compressed_size);
    _cairo_output_stream_printf (output, "\r\n");
    _cairo_pdf_document_close_stream (document);

 CLEANUP_COMPRESSED:
    free (compressed);
 CLEANUP_RGB:
    free (rgb);
 CLEANUP:
    return status;
}

static cairo_status_t
emit_solid_pattern (cairo_pdf_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t alpha;

    alpha = _cairo_pdf_surface_add_alpha (surface, pattern->color.alpha);

    /* With some work, we could separate the stroking
     * or non-stroking color here as actually needed. */
    _cairo_output_stream_printf (output,
				 "%f %f %f RG "
				 "%f %f %f rg "
				 "/a%d gs\r\n",
				 pattern->color.red,
				 pattern->color.green,
				 pattern->color.blue,
				 pattern->color.red,
				 pattern->color.green,
				 pattern->color.blue,
				 alpha.id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
emit_surface_pattern (cairo_pdf_surface_t	*dst,
		      cairo_surface_pattern_t	*pattern)
{
    cairo_pdf_document_t *document = dst->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t stream;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_resource_t alpha, image_resource;
    cairo_matrix_t cairo_p2d, pdf_p2d;
    cairo_extend_t extend = cairo_pattern_get_extend (&pattern->base);
    int xstep, ystep;
    cairo_rectangle_fixed_t dst_extents;

    /* XXX: Should do something clever here for PDF source surfaces ? */

    _cairo_pdf_surface_pause_content_stream (dst);

    status = _cairo_surface_acquire_source_image (pattern->surface, &image, &image_extra);
    if (status)
	return status;

    status = emit_image (dst->document, image, &image_resource);
    if (status)
	goto BAIL;

    _cairo_surface_get_extents (&dst->base, &dst_extents);

    /* In PDF, (as far as I can tell), all patterns are repeating. So
     * we support cairo's EXTEND_NONE semantics by setting the repeat
     * step size to the larger of the image size and the extents of
     * the destination surface. That way we guarantee the pattern will
     * not repeat.
     */
    switch (extend) {
    case CAIRO_EXTEND_NONE:
	xstep = MAX(image->width, dst_extents.width);
	ystep = MAX(image->height, dst_extents.height);
	break;
    case CAIRO_EXTEND_REPEAT:
	xstep = image->width;
	ystep = image->height;
	break;
    default:
	ASSERT_NOT_REACHED; /* all others should be analyzed away */
	xstep = 0;
	ystep = 0;
    }

    /* At this point, (that is, within the surface backend interface),
     * the pattern's matrix maps from cairo's device space to cairo's
     * pattern space, (both with their origin at the upper-left, and
     * cairo's pattern space of size width,height).
     *
     * Then, we must emit a PDF pattern object that maps from its own
     * pattern space, (which has a size that we establish in the BBox
     * dictionary entry), to the PDF page's *initial* space, (which
     * does not benefit from the Y-axis flipping matrix that we emit
     * on each page). So the PDF patterns patrix maps from a
     * (width,height) pattern space to a device space with the origin
     * in the lower-left corner.
     *
     * So to handle all of that, we start with an identity matrix for
     * the PDF pattern to device matrix. We translate it up by the
     * image height then flip it in the Y direction, (moving us from
     * the PDF origin to cairo's origin). We then multiply in the
     * inverse of the cairo pattern matrix, (since it maps from device
     * to pattern, while we're setting up pattern to device). Finally,
     * we translate back down by the image height and flip again to
     * end up at the lower-left origin that PDF expects.
     *
     * Additionally, within the stream that paints the pattern itself,
     * we are using a PDF image object that has a size of (1,1) so we
     * have to scale it up by the image width and height to fill our
     * pattern cell.
     */
    cairo_p2d = pattern->base.matrix;
    cairo_matrix_invert (&cairo_p2d);

    cairo_matrix_init_identity (&pdf_p2d);
    cairo_matrix_translate (&pdf_p2d, 0.0, dst_extents.height);
    cairo_matrix_scale (&pdf_p2d, 1.0, -1.0);
    cairo_matrix_multiply (&pdf_p2d, &cairo_p2d, &pdf_p2d);
    cairo_matrix_translate (&pdf_p2d, 0.0, image->height);
    cairo_matrix_scale (&pdf_p2d, 1.0, -1.0);

    stream = _cairo_pdf_document_open_stream (document,
					      "   /BBox [0 0 %d %d]\r\n"
					      "   /XStep %d\r\n"
					      "   /YStep %d\r\n"
					      "   /PatternType 1\r\n"
					      "   /TilingType 1\r\n"
					      "   /PaintType 1\r\n"
					      "   /Matrix [ %f %f %f %f %f %f ]\r\n"
					      "   /Resources << /XObject << /res%d %d 0 R >> >>\r\n",
					      image->width, image->height,
					      xstep, ystep,
					      pdf_p2d.xx, pdf_p2d.yx,
					      pdf_p2d.xy, pdf_p2d.yy,
					      pdf_p2d.x0, pdf_p2d.y0,
					      image_resource.id,
					      image_resource.id);

    _cairo_output_stream_printf (output,
				 "q %d 0 0 %d 0 0 cm /res%d Do Q\r\n",
				 image->width, image->height,
				 image_resource.id);

    _cairo_pdf_document_close_stream (document);

    _cairo_pdf_surface_resume_content_stream (dst);

    _cairo_pdf_surface_add_pattern (dst, stream);

    alpha = _cairo_pdf_surface_add_alpha (dst, 1.0);
    /* With some work, we could separate the stroking
     * or non-stroking pattern here as actually needed. */
    _cairo_output_stream_printf (output,
				 "/Pattern CS /res%d SCN "
				 "/Pattern cs /res%d scn "
				 "/a%d gs\r\n",
				 stream.id, stream.id, alpha.id);

 BAIL:
    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return status;
}


typedef struct _cairo_pdf_color_stop {
    double	  		offset;
    cairo_pdf_resource_t	gradient;
    unsigned char		color_char[4];
} cairo_pdf_color_stop_t;

static cairo_pdf_resource_t
emit_linear_colorgradient (cairo_pdf_document_t   *document,
			   cairo_pdf_color_stop_t *stop1, 
			   cairo_pdf_color_stop_t *stop2)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t function = _cairo_pdf_document_new_object (document);
    
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 0\r\n"
				 "   /Domain [ 0 1 ]\r\n"
				 "   /Size [ 2 ]\r\n"
				 "   /BitsPerSample 8\r\n"
				 "   /Range [ 0 1 0 1 0 1 ]\r\n"
				 "   /Length 6\r\n"
				 ">>\r\n"
				 "stream\r\n",
				 function.id);

    _cairo_output_stream_write (output, stop1->color_char, 3);
    _cairo_output_stream_write (output, stop2->color_char, 3);
    _cairo_output_stream_printf (output,
				 "\r\n"
				 "endstream\r\n"
				 "endobj\r\n");
    
    return function;
}

static cairo_pdf_resource_t
emit_stiched_colorgradient (cairo_pdf_document_t   *document,
			    unsigned int 	   n_stops,
			    cairo_pdf_color_stop_t stops[])
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t function;
    unsigned int i;
    
    /* emit linear gradients between pairs of subsequent stops... */
    for (i = 0; i < n_stops-1; i++) {
	stops[i].gradient = emit_linear_colorgradient (document, 
						       &stops[i], 
						       &stops[i+1]);
    }
    
    /* ... and stich them together */
    function = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 3\r\n"
				 "   /Domain [ 0 1 ]\r\n"
				 "   /Functions [ ",
				 function.id);
    for (i = 0; i < n_stops-1; i++)
        _cairo_output_stream_printf (output, "%d 0 R ", stops[i].gradient.id);
    _cairo_output_stream_printf (output,
		    		 "]\r\n"
				 "   /Bounds [ ");
    for (i = 1; i < n_stops-1; i++)
        _cairo_output_stream_printf (output, "%f ", stops[i].offset);
    _cairo_output_stream_printf (output,
		    		 "]\r\n"
				 "   /Encode [ ");
    for (i = 1; i < n_stops; i++)
        _cairo_output_stream_printf (output, "0 1 ");
    _cairo_output_stream_printf (output, 
	    			 "]\r\n"
	    			 ">>\r\n"
				 "endobj\r\n");

    return function;
}

#define COLOR_STOP_EPSILLON 1e-6

static cairo_pdf_resource_t
emit_pattern_stops (cairo_pdf_surface_t *surface, cairo_gradient_pattern_t *pattern)
{
    cairo_pdf_document_t   *document = surface->document;
    cairo_pdf_resource_t    function;
    cairo_pdf_color_stop_t *allstops, *stops;
    unsigned int 	   n_stops;
    unsigned int 	   i;

    function = _cairo_pdf_document_new_object (document);

    allstops = malloc ((pattern->n_stops + 2) * sizeof (cairo_pdf_color_stop_t));
    if (allstops == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	function.id = 0;
	return function;
    }
    stops = &allstops[1];
    n_stops = pattern->n_stops;
    
    for (i = 0; i < pattern->n_stops; i++) {
	stops[i].color_char[0] = pattern->stops[i].color.red   >> 8;
	stops[i].color_char[1] = pattern->stops[i].color.green >> 8;
	stops[i].color_char[2] = pattern->stops[i].color.blue  >> 8;
	stops[i].color_char[3] = pattern->stops[i].color.alpha >> 8;
	stops[i].offset = _cairo_fixed_to_double (pattern->stops[i].x);
    }

    /* make sure first offset is 0.0 and last offset is 1.0. (Otherwise Acrobat
     * Reader chokes.) */
    if (stops[0].offset > COLOR_STOP_EPSILLON) {
	    memcpy (allstops, stops, sizeof (cairo_pdf_color_stop_t));
	    stops = allstops;
	    stops[0].offset = 0.0;
	    n_stops++;
    }
    if (stops[n_stops-1].offset < 1.0 - COLOR_STOP_EPSILLON) {
	    memcpy (&stops[n_stops], 
		    &stops[n_stops - 1], 
		    sizeof (cairo_pdf_color_stop_t));
	    stops[n_stops].offset = 1.0;
	    n_stops++;
    }
    
    if (n_stops == 2) {
	/* no need for stiched function */
	function = emit_linear_colorgradient (document, &stops[0], &stops[1]);
    } else {
	/* multiple stops: stich. XXX possible optimization: regulary spaced
	 * stops do not require stiching. XXX */
	function = emit_stiched_colorgradient (document, 
					       n_stops, 
					       stops);
    }

    free (allstops);

    return function;
}

static cairo_status_t
emit_linear_pattern (cairo_pdf_surface_t *surface, cairo_linear_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t function, pattern_resource, alpha;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;

    _cairo_pdf_surface_pause_content_stream (surface);

    function = emit_pattern_stops (surface, &pattern->base);
    if (function.id == 0)
	return CAIRO_STATUS_NO_MEMORY;

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    x0 = _cairo_fixed_to_double (pattern->gradient.p1.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.p1.y);
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = _cairo_fixed_to_double (pattern->gradient.p2.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.p2.y);
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    pattern_resource = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Pattern\r\n"
				 "   /PatternType 2\r\n"
				 "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
				 "   /Shading\r\n"
				 "      << /ShadingType 2\r\n"
				 "         /ColorSpace /DeviceRGB\r\n"
				 "         /Coords [ %f %f %f %f ]\r\n"
				 "         /Function %d 0 R\r\n"
				 "         /Extend [ true true ]\r\n"
				 "      >>\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 pattern_resource.id,
				 document->height,
				 x0, y0, x1, y1,
				 function.id);
    
    _cairo_pdf_surface_add_pattern (surface, pattern_resource);

    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    /* With some work, we could separate the stroking
     * or non-stroking pattern here as actually needed. */
    _cairo_output_stream_printf (output,
				 "/Pattern CS /res%d SCN "
				 "/Pattern cs /res%d scn "
				 "/a%d gs\r\n",
				 pattern_resource.id,
				 pattern_resource.id,
				 alpha.id);

    _cairo_pdf_surface_resume_content_stream (surface);

    return CAIRO_STATUS_SUCCESS;
}
	
static cairo_status_t
emit_radial_pattern (cairo_pdf_surface_t *surface, cairo_radial_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t function, pattern_resource, alpha;
    double x0, y0, x1, y1, r0, r1;
    cairo_matrix_t p2u;

    _cairo_pdf_surface_pause_content_stream (surface);

    function = emit_pattern_stops (surface, &pattern->base);
    if (function.id == 0)
	return CAIRO_STATUS_NO_MEMORY;

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    x0 = _cairo_fixed_to_double (pattern->gradient.inner.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.inner.y);
    r0 = _cairo_fixed_to_double (pattern->gradient.inner.radius);
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = _cairo_fixed_to_double (pattern->gradient.outer.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.outer.y);
    r1 = _cairo_fixed_to_double (pattern->gradient.outer.radius);
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    /* FIXME: This is surely crack, but how should you scale a radius
     * in a non-orthogonal coordinate system? */
    cairo_matrix_transform_distance (&p2u, &r0, &r1);

    /* FIXME: There is a difference between the cairo gradient extend
     * semantics and PDF extend semantics. PDFs extend=false means
     * that nothing is painted outside the gradient boundaries,
     * whereas cairo takes this to mean that the end color is padded
     * to infinity. Setting extend=true in PDF gives the cairo default
     * behavoir, not yet sure how to implement the cairo mirror and
     * repeat behaviour. */
    pattern_resource = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Pattern\r\n"
				 "   /PatternType 2\r\n"
				 "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
				 "   /Shading\r\n"
				 "      << /ShadingType 3\r\n"
				 "         /ColorSpace /DeviceRGB\r\n"
				 "         /Coords [ %f %f %f %f %f %f ]\r\n"
				 "         /Function %d 0 R\r\n"
				 "         /Extend [ true true ]\r\n"
				 "      >>\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 pattern_resource.id,
				 document->height,
				 x0, y0, r0, x1, y1, r1,
				 function.id);
    
    _cairo_pdf_surface_add_pattern (surface, pattern_resource);

    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    /* With some work, we could separate the stroking
     * or non-stroking pattern here as actually needed. */
    _cairo_output_stream_printf (output,
				 "/Pattern CS /res%d SCN "
				 "/Pattern cs /res%d scn "
				 "/a%d gs\r\n",
				 pattern_resource.id,
				 pattern_resource.id,
				 alpha.id);

    _cairo_pdf_surface_resume_content_stream (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
emit_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);

    }

    ASSERT_NOT_REACHED;
    return CAIRO_STATUS_PATTERN_TYPE_MISMATCH;
}

static cairo_status_t
_cairo_pdf_path_move_to (void *closure, cairo_point_t *point)
{
    cairo_output_stream_t *output_stream = closure;

    _cairo_output_stream_printf (output_stream,
				 "%f %f m ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_line_to (void *closure, cairo_point_t *point)
{
    cairo_output_stream_t *output_stream = closure;
    
    _cairo_output_stream_printf (output_stream,
				 "%f %f l ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    cairo_output_stream_t *output_stream = closure;

    _cairo_output_stream_printf (output_stream,
				 "%f %f %f %f %f %f c ",
				 _cairo_fixed_to_double (b->x),
				 _cairo_fixed_to_double (b->y),
				 _cairo_fixed_to_double (c->x),
				 _cairo_fixed_to_double (c->y),
				 _cairo_fixed_to_double (d->x),
				 _cairo_fixed_to_double (d->y));
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_close_path (void *closure)
{
    cairo_output_stream_t *output_stream = closure;
    
    _cairo_output_stream_printf (output_stream,
				 "h\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_copy_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    return _cairo_pdf_document_add_page (document, surface);
}

static cairo_int_status_t
_cairo_pdf_surface_show_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_int_status_t status;

    status = _cairo_pdf_document_add_page (document, surface);
    if (status)
	return status;

    _cairo_pdf_surface_clear (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_fixed_t *rectangle)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_size.
     */
    rectangle->width  = (int) ceil (surface->width);
    rectangle->height = (int) ceil (surface->height);

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _pdf_stroke {
    cairo_output_stream_t   *output_stream;
    cairo_matrix_t	    *ctm_inverse;
} pdf_stroke_t;

static cairo_status_t
_cairo_pdf_stroke_move_to (void *closure, cairo_point_t *point)
{
    pdf_stroke_t *stroke = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    cairo_matrix_transform_point (stroke->ctm_inverse, &x, &y);

    _cairo_output_stream_printf (stroke->output_stream,
				 "%f %f m ", x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_stroke_line_to (void *closure, cairo_point_t *point)
{
    pdf_stroke_t *stroke = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    cairo_matrix_transform_point (stroke->ctm_inverse, &x, &y);

    _cairo_output_stream_printf (stroke->output_stream,
				 "%f %f l ", x, y);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_stroke_curve_to (void          *closure,
			    cairo_point_t *b,
			    cairo_point_t *c,
			    cairo_point_t *d)
{
    pdf_stroke_t *stroke = closure;
    double bx = _cairo_fixed_to_double (b->x);
    double by = _cairo_fixed_to_double (b->y);
    double cx = _cairo_fixed_to_double (c->x);
    double cy = _cairo_fixed_to_double (c->y);
    double dx = _cairo_fixed_to_double (d->x);
    double dy = _cairo_fixed_to_double (d->y);

    cairo_matrix_transform_point (stroke->ctm_inverse, &bx, &by);
    cairo_matrix_transform_point (stroke->ctm_inverse, &cx, &cy);
    cairo_matrix_transform_point (stroke->ctm_inverse, &dx, &dy);

    _cairo_output_stream_printf (stroke->output_stream,
				 "%f %f %f %f %f %f c ",
				 bx, by, cx, cy, dx, dy);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_stroke_close_path (void *closure)
{
    pdf_stroke_t *stroke = closure;

    _cairo_output_stream_printf (stroke->output_stream,
				 "h\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_intersect_clip_path (void			*dst,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	fill_rule,
					double			tolerance,
					cairo_antialias_t	antialias)
{
    cairo_pdf_surface_t *surface = dst;
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_status_t status;
    const char *pdf_operator;

    if (path == NULL) {
	if (surface->has_clip)
	    _cairo_output_stream_printf (output, "Q\r\n");
	surface->has_clip = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }

    if (!surface->has_clip) {
	_cairo_output_stream_printf (output, "q ");
	surface->has_clip = TRUE;
    }

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  document->output_stream);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	pdf_operator = "W";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	pdf_operator = "W*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (document->output_stream,
				 "%s n\r\n",
				 pdf_operator);

    return status;
}

static void
_cairo_pdf_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
}

static cairo_pdf_document_t *
_cairo_pdf_document_create (cairo_output_stream_t	*output_stream,
			    double			width,
			    double			height)
{
    cairo_pdf_document_t *document;

    document = malloc (sizeof (cairo_pdf_document_t));
    if (document == NULL)
	return NULL;

    document->output_stream = output_stream;
    document->ref_count = 1;
    document->owner = NULL;
    document->finished = FALSE;
    document->width = width;
    document->height = height;
    document->x_dpi = DEFAULT_DPI;
    document->y_dpi = DEFAULT_DPI;

    _cairo_array_init (&document->objects, sizeof (cairo_pdf_object_t));
    _cairo_array_init (&document->pages, sizeof (cairo_pdf_resource_t));
    document->next_available_resource.id = 1;

    document->current_stream.active = FALSE;

    document->pages_resource = _cairo_pdf_document_new_object (document);

    _cairo_array_init (&document->fonts, sizeof (cairo_font_subset_t *));

    /* Document header */
    _cairo_output_stream_printf (output_stream,
				 "%%PDF-1.4\r\n");

    return document;
}

static cairo_pdf_resource_t
_cairo_pdf_document_write_info (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t info;

    info = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Creator (cairographics.org)\r\n"
				 "   /Producer (cairographics.org)\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 info.id);

    return info;
}

static void
_cairo_pdf_document_write_pages (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *stream = document->output_stream;
    cairo_pdf_resource_t page;
    int num_pages, i;

    _cairo_pdf_document_update_object (document, document->pages_resource);
    _cairo_output_stream_printf (stream,
				 "%d 0 obj\r\n"
				 "<< /Type /Pages\r\n"
				 "   /Kids [ ",
				 document->pages_resource.id);
    
    num_pages = _cairo_array_num_elements (&document->pages);
    for (i = 0; i < num_pages; i++) {
	_cairo_array_copy_element (&document->pages, i, &page);
	_cairo_output_stream_printf (stream, "%d 0 R ", page.id);
    }

    _cairo_output_stream_printf (stream, "]\r\n"); 
    _cairo_output_stream_printf (stream, "   /Count %d\r\n", num_pages);

    /* TODO: Figure out wich other defaults to be inherited by /Page
     * objects. */
    _cairo_output_stream_printf (stream,
				 "   /MediaBox [ 0 0 %f %f ]\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 document->width,
				 document->height);
}

static cairo_status_t
_cairo_pdf_document_write_fonts (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_font_subset_t *font;
    cairo_pdf_resource_t font_resource;
    int num_fonts, i, j;
    const char *data;
    char *compressed;
    unsigned long data_size, compressed_size;
    cairo_pdf_resource_t stream, descriptor;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    num_fonts = _cairo_array_num_elements (&document->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&document->fonts, i, &font);

	status = _cairo_font_subset_generate (font, &data, &data_size);
	if (status)
	    goto fail;

	compressed = compress_dup (data, data_size, &compressed_size);
	if (compressed == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto fail;
	}

	stream = _cairo_pdf_document_new_object (document);
	_cairo_output_stream_printf (output,
				     "%d 0 obj\r\n"
				     "<< /Filter /FlateDecode\r\n"
				     "   /Length %lu\r\n"
				     "   /Length1 %lu\r\n"
				     ">>\r\n"
				     "stream\r\n",
				     stream.id,
				     compressed_size,
				     data_size);
	_cairo_output_stream_write (output, compressed, compressed_size);
	_cairo_output_stream_printf (output,
				     "\r\n"
				     "endstream\r\n"
				     "endobj\r\n");
	free (compressed);

	descriptor = _cairo_pdf_document_new_object (document);
	_cairo_output_stream_printf (output,
				     "%d 0 obj\r\n"
				     "<< /Type /FontDescriptor\r\n"
				     "   /FontName /7%s\r\n"
				     "   /Flags 4\r\n"
				     "   /FontBBox [ %ld %ld %ld %ld ]\r\n"
				     "   /ItalicAngle 0\r\n"
				     "   /Ascent %ld\r\n"
				     "   /Descent %ld\r\n"
				     "   /CapHeight 500\r\n"
				     "   /StemV 80\r\n"
				     "   /StemH 80\r\n"
				     "   /FontFile2 %u 0 R\r\n"
				     ">>\r\n"
				     "endobj\r\n",
				     descriptor.id,
				     font->base_font,
				     font->x_min,
				     font->y_min,
				     font->x_max,
				     font->y_max,
				     font->ascent,
				     font->descent,
				     stream.id);

	font_resource.id = font->font_id;
	_cairo_pdf_document_update_object (document, font_resource);
	_cairo_output_stream_printf (output,
				     "%d 0 obj\r\n"
				     "<< /Type /Font\r\n"
				     "   /Subtype /TrueType\r\n"
				     "   /BaseFont /%s\r\n"
				     "   /FirstChar 0\r\n"
				     "   /LastChar %d\r\n"
				     "   /FontDescriptor %d 0 R\r\n"
				     "   /Widths ",
				     font->font_id,
				     font->base_font,
				     font->num_glyphs,
				     descriptor.id);

	_cairo_output_stream_printf (output,
				     "[");

	for (j = 0; j < font->num_glyphs; j++)
	    _cairo_output_stream_printf (output,
					 " %d",
					 font->widths[j]);

	_cairo_output_stream_printf (output,
				     " ]\r\n"
				     ">>\r\n"
				     "endobj\r\n");

    fail:
	_cairo_font_subset_destroy (font);
    }

    return status;
}

static cairo_pdf_resource_t
_cairo_pdf_document_write_catalog (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t catalog;

    catalog = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Catalog\r\n"
				 "   /Pages %d 0 R\r\n" 
				 ">>\r\n"
				 "endobj\r\n",
				 catalog.id,
				 document->pages_resource.id);

    return catalog;
}

static long
_cairo_pdf_document_write_xref (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_object_t *object;
    int num_objects, i;
    long offset;
    char buffer[11];

    num_objects = _cairo_array_num_elements (&document->objects);

    offset = _cairo_output_stream_get_position (output);
    _cairo_output_stream_printf (output,
				 "xref\r\n"
				 "%d %d\r\n",
				 0, num_objects + 1);

    _cairo_output_stream_printf (output,
				 "0000000000 65535 f\r\n");
    for (i = 0; i < num_objects; i++) {
	object = _cairo_array_index (&document->objects, i);
	snprintf (buffer, sizeof buffer, "%010ld", object->offset);
	_cairo_output_stream_printf (output,
				     "%s 00000 n\r\n", buffer);
    }

    return offset;
}

static cairo_pdf_document_t *
_cairo_pdf_document_reference (cairo_pdf_document_t *document)
{
    document->ref_count++;

    return document;
}

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document)
{
    document->ref_count--;
    if (document->ref_count > 0)
      return;

    _cairo_pdf_document_finish (document);

    free (document);
}
    
static cairo_status_t
_cairo_pdf_document_finish (cairo_pdf_document_t *document)
{
    cairo_status_t status;
    cairo_output_stream_t *output = document->output_stream;
    long offset;
    cairo_pdf_resource_t info, catalog;

    if (document->finished)
	return CAIRO_STATUS_SUCCESS;

    _cairo_pdf_document_close_stream (document);
    _cairo_pdf_document_write_pages (document);
    _cairo_pdf_document_write_fonts (document);
    info = _cairo_pdf_document_write_info (document);
    catalog = _cairo_pdf_document_write_catalog (document);
    offset = _cairo_pdf_document_write_xref (document);
    
    _cairo_output_stream_printf (output,
				 "trailer\r\n"
				 "<< /Size %d\r\n"
				 "   /Root %d 0 R\r\n"
				 "   /Info %d 0 R\r\n"
				 ">>\r\n",
				 document->next_available_resource.id,
				 catalog.id,
				 info.id);

    _cairo_output_stream_printf (output,
				 "startxref\r\n"
				 "%ld\r\n"
				 "%%%%EOF\r\n",
				 offset);

    status = _cairo_output_stream_get_status (output);
    _cairo_output_stream_destroy (output);

    _cairo_array_fini (&document->objects);
    _cairo_array_fini (&document->pages);
    _cairo_array_fini (&document->fonts);

    document->finished = TRUE;

    return status;
}

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t	*document,
			      cairo_pdf_surface_t	*surface)
{
    cairo_status_t status;
    cairo_pdf_resource_t *res;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_resource_t page;
    double alpha;
    cairo_pdf_resource_t stream;
    int num_streams, num_alphas, num_resources, i;

    assert (!document->finished);

    if (surface->has_clip) {
	_cairo_output_stream_printf (output, "Q\r\n");
	surface->has_clip = FALSE;
    }

    _cairo_pdf_document_close_stream (document);

    page = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Page\r\n"
				 "   /Parent %d 0 R\r\n",
				 page.id,
				 document->pages_resource.id);

    if (surface->width != document->width ||
	surface->height != document->height)
    {
	_cairo_output_stream_printf (output,
				     "   /MediaBox [ 0 0 %f %f ]\r\n",
				     surface->width,
				     surface->height);
    }

    _cairo_output_stream_printf (output,
				 "   /Contents [");
    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);
	_cairo_output_stream_printf (output,
				     " %d 0 R",
				     stream.id);
    }
    _cairo_output_stream_printf (output,
				 " ]\r\n");

    _cairo_output_stream_printf (output,
				 "   /Resources <<\r\n");

    num_resources =  _cairo_array_num_elements (&surface->fonts);
    if (num_resources > 0) {
	_cairo_output_stream_printf (output,
				     "      /Font <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->fonts, i);
	    _cairo_output_stream_printf (output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (output,
				     " >>\r\n");
    }
    
    num_alphas =  _cairo_array_num_elements (&surface->alphas);
    if (num_alphas > 0) {
	_cairo_output_stream_printf (output,
				     "      /ExtGState <<\r\n");

	for (i = 0; i < num_alphas; i++) {
	    /* With some work, we could separate the stroking
	     * or non-stroking alpha here as actually needed. */
	    _cairo_array_copy_element (&surface->alphas, i, &alpha);
	    _cairo_output_stream_printf (output,
					 "         /a%d << /CA %f /ca %f >>\r\n",
					 i, alpha, alpha);
	}

	_cairo_output_stream_printf (output,
				     "      >>\r\n");
    }
    
    num_resources = _cairo_array_num_elements (&surface->patterns);
    if (num_resources > 0) {
	_cairo_output_stream_printf (output,
				     "      /Pattern <<");
	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->patterns, i);
	    _cairo_output_stream_printf (output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (output,
				     " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    if (num_resources > 0) {
	_cairo_output_stream_printf (output,
				     "      /XObject <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->xobjects, i);
	    _cairo_output_stream_printf (output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (output,
				     " >>\r\n");
    }

    _cairo_output_stream_printf (output,
				 "   >>\r\n"
				 ">>\r\n"
				 "endobj\r\n");

    status = _cairo_array_append (&document->pages, &page);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_surface_pattern_supported (cairo_surface_pattern_t *pattern)
{
    cairo_extend_t extend;

    if (pattern->surface->backend->acquire_source_image == NULL)
	return FALSE;

    /* Does an ALPHA-only source surface even make sense? Maybe, but I
     * don't think it's worth the extra code to support it. */

/* XXX: Need to write this function here...
    content = cairo_surface_get_content (pattern->surface);
    if (content == CAIRO_CONTENT_ALPHA)
	return FALSE;
*/

    extend = cairo_pattern_get_extend (&pattern->base);
    switch (extend) {
    case CAIRO_EXTEND_NONE:
    case CAIRO_EXTEND_REPEAT:
	return TRUE;
    case CAIRO_EXTEND_REFLECT:
    case CAIRO_EXTEND_PAD:
	return FALSE;
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_bool_t
_pattern_supported (cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
	return _surface_pattern_supported ((cairo_surface_pattern_t *) pattern);
	
    return FALSE;
}

static cairo_int_status_t
_operation_supported (cairo_pdf_surface_t *surface,
		      cairo_operator_t op,
		      cairo_pattern_t *pattern)
{
    if (! _pattern_supported (pattern))
	return FALSE;

    /* XXX: We can probably support a fair amount more than just OVER,
     * but this should cover many common cases at least. */
    if (op == CAIRO_OPERATOR_OVER)
	return TRUE;

    return FALSE;
}

static cairo_int_status_t
_analyze_operation (cairo_pdf_surface_t *surface,
		    cairo_operator_t op,
		    cairo_pattern_t *pattern)
{
    if (_operation_supported (surface, op, pattern))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_pdf_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    /* XXX: It would be nice to be able to assert this condition
     * here. But, we actually allow one 'cheat' that is used when
     * painting the final image-based fallbacks. The final fallbacks
     * do have alpha which we support by blending with white. This is
     * possible only because there is nothing between the fallback
     * images and the paper, nor is anything painted above. */
    /*
    assert (_operation_supported (op, source));
    */

    status = emit_pattern (surface, source);
    if (status)
	return status;

    _cairo_output_stream_printf (document->output_stream,
				 "0 0 %f %f re f\r\n",
				 surface->width, surface->height);

    return _cairo_output_stream_get_status (document->output_stream);
}

static cairo_int_status_t
_cairo_pdf_surface_mask	(void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_pattern_t	*mask)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    ASSERT_NOT_REACHED;

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static int
_cairo_pdf_line_cap (cairo_line_cap_t cap)
{
    switch (cap) {
    case CAIRO_LINE_CAP_BUTT:
	return 0;
    case CAIRO_LINE_CAP_ROUND:
	return 1;
    case CAIRO_LINE_CAP_SQUARE:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static int
_cairo_pdf_line_join (cairo_line_join_t join)
{
    switch (join) {
    case CAIRO_LINE_JOIN_MITER:
	return 0;
    case CAIRO_LINE_JOIN_ROUND:
	return 1;
    case CAIRO_LINE_JOIN_BEVEL:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static cairo_status_t
_cairo_pdf_surface_emit_stroke_style (cairo_pdf_surface_t	*surface,
				      cairo_output_stream_t	*stream,
				      cairo_stroke_style_t	*style)
{
    _cairo_output_stream_printf (stream,
				 "%f w\r\n",
				 style->line_width);

    _cairo_output_stream_printf (stream,
				 "%d J\r\n",
				 _cairo_pdf_line_cap (style->line_cap));

    _cairo_output_stream_printf (stream, 
				 "%d j\r\n",
				 _cairo_pdf_line_join (style->line_join));

    if (style->num_dashes) {
	int d;
	_cairo_output_stream_printf (stream, "[");
	for (d = 0; d < style->num_dashes; d++)
	    _cairo_output_stream_printf (stream, " %f", style->dash[d]);
	_cairo_output_stream_printf (stream, "] %f d\r\n",
				     style->dash_offset);
    }

    _cairo_output_stream_printf (stream,
				 "%f M ",
				 style->miter_limit);

    return _cairo_output_stream_get_status (stream);
}

static cairo_int_status_t
_cairo_pdf_surface_stroke (void			*abstract_surface,
			   cairo_operator_t	 op,
			   cairo_pattern_t	*source,
			   cairo_path_fixed_t	*path,
			   cairo_stroke_style_t	*style,
			   cairo_matrix_t	*ctm,
			   cairo_matrix_t	*ctm_inverse,
			   double		 tolerance,
			   cairo_antialias_t	 antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    pdf_stroke_t stroke;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	/* XXX: Does PDF provide a way we can preserve this hint? For now,
	 * this will trigger a fallback. */
	if (antialias == CAIRO_ANTIALIAS_NONE)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	return _analyze_operation (surface, op, source);
    }

    assert (_operation_supported (surface, op, source));

    status = emit_pattern (surface, source);
    if (status)
	return status;

    stroke.output_stream = document->output_stream;
    stroke.ctm_inverse = ctm_inverse;
    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_stroke_move_to,
					  _cairo_pdf_stroke_line_to,
					  _cairo_pdf_stroke_curve_to,
					  _cairo_pdf_stroke_close_path,
					  &stroke);
				 
    _cairo_output_stream_printf (document->output_stream,
				 "q %f %f %f %f %f %f cm\r\n",
				 ctm->xx, ctm->yx, ctm->xy, ctm->yy,
				 ctm->x0, ctm->y0);

    status = _cairo_pdf_surface_emit_stroke_style (surface,
						   document->output_stream,
						   style);
    if (status)
	return status;

    _cairo_output_stream_printf (document->output_stream, "S Q\r\n");

    return status;
}

static cairo_int_status_t
_cairo_pdf_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    const char *pdf_operator;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	/* XXX: Does PDF provide a way we can preserve this hint? For now,
	 * this will trigger a fallback. */
	if (antialias == CAIRO_ANTIALIAS_NONE)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	return _analyze_operation (surface, op, source);
    }

    assert (_operation_supported (surface, op, source));

    status = emit_pattern (surface, source);
    if (status)
	return status;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  document->output_stream);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	pdf_operator = "f";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	pdf_operator = "f*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (document->output_stream,
				 "%s\r\n",
				 pdf_operator);

    return status;
}

static cairo_int_status_t
_cairo_pdf_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				const cairo_glyph_t	*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_path_fixed_t path;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (_operation_supported (surface, op, source));

    status = emit_pattern (surface, source);
    if (status)
	return status;

    _cairo_path_fixed_init (&path);
    _cairo_scaled_font_glyph_path (scaled_font, glyphs, num_glyphs, &path);
    status = _cairo_pdf_surface_fill (surface, op, source,
				      &path, CAIRO_FILL_RULE_WINDING,
				      0.1, scaled_font->options.antialias);
    _cairo_path_fixed_fini (&path);

    return status;
}

static void
_cairo_pdf_surface_set_paginated_mode (void			*abstract_surface,
				       cairo_paginated_mode_t	 paginated_mode)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;
}

static const cairo_surface_backend_t cairo_pdf_surface_backend = {
    CAIRO_SURFACE_TYPE_PDF,
    _cairo_pdf_surface_create_similar,
    _cairo_pdf_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    _cairo_pdf_surface_copy_page,
    _cairo_pdf_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_pdf_surface_intersect_clip_path,
    _cairo_pdf_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_pdf_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the drawing functions */

    _cairo_pdf_surface_paint,
    _cairo_pdf_surface_mask,
    _cairo_pdf_surface_stroke,
    _cairo_pdf_surface_fill,
    _cairo_pdf_surface_show_glyphs,
    NULL, /* snapshot */
};

static const cairo_paginated_surface_backend_t cairo_pdf_surface_paginated_backend = {
    _cairo_pdf_surface_start_page,
    _cairo_pdf_surface_set_paginated_mode
};
