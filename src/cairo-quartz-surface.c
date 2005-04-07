/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Calum Robinson
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
 * The Initial Developer of the Original Code is Calum Robinson
 *
 * Contributor(s):
 *    Calum Robinson <calumr@mac.com>
 */

#include "cairoint.h"
#include "cairo-private.h"
#include "cairo-quartz.h"

typedef struct cairo_quartz_surface {
    cairo_surface_t base;

    CGContextRef context;

    int width;
    int height;

    cairo_image_surface_t *image;

    CGImageRef cgImage;
} cairo_quartz_surface_t;

static void
ImageDataReleaseFunc(void *info, const void *data, size_t size)
{
    if (data != NULL) {
        free((void *) data);
    }
}


void
cairo_set_target_quartz_context(cairo_t * cr,
                                CGContextRef context,
                                int width, int height)
{
    cairo_surface_t *surface;


    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
        return;

    surface = cairo_quartz_surface_create(context, width, height);
    if (surface == NULL) {
        cr->status = CAIRO_STATUS_NO_MEMORY;
        return;
    }

    cairo_set_target_surface(cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy(surface);
}


static cairo_surface_t *_cairo_quartz_surface_create_similar(void
                                                             *abstract_src,
                                                             cairo_format_t
                                                             format,
                                                             int drawable,
                                                             int width,
                                                             int height)
{
    return NULL;
}


static void _cairo_quartz_surface_finish(void *abstract_surface)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    if (surface->image)
        cairo_surface_destroy(surface->image);

    if (surface->cgImage)
        CGImageRelease(surface->cgImage);
}

static cairo_status_t
_cairo_quartz_surface_acquire_source_image(void *abstract_surface,
					   cairo_image_surface_t **image_out,
					   void **image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;
    CGColorSpaceRef colorSpace;
    void *imageData;
    UInt32 imageDataSize, rowBytes;
    CGDataProviderRef dataProvider;

    // We keep a cached (cairo_image_surface_t *) in the cairo_quartz_surface_t
    // struct. If the window is ever drawn to without going through Cairo, then
    // we would need to refetch the pixel data from the window into the cached
    // image surface. 
    if (surface->image) {
        cairo_surface_reference(&surface->image->base);

	*image_out = surface->image;
	return CAIRO_STATUS_SUCCESS;
    }

    colorSpace = CGColorSpaceCreateDeviceRGB();


    rowBytes = surface->width * 4;
    imageDataSize = rowBytes * surface->height;
    imageData = malloc(imageDataSize);

    dataProvider =
        CGDataProviderCreateWithData(NULL, imageData, imageDataSize,
                                     ImageDataReleaseFunc);

    surface->cgImage = CGImageCreate(surface->width,
                                     surface->height,
                                     8,
                                     32,
                                     rowBytes,
                                     colorSpace,
                                     kCGImageAlphaPremultipliedFirst,
                                     dataProvider,
                                     NULL,
                                     false, kCGRenderingIntentDefault);

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(dataProvider);

    surface->image = (cairo_image_surface_t *)
        cairo_image_surface_create_for_data(imageData,
                                            CAIRO_FORMAT_ARGB32,
                                            surface->width,
                                            surface->height, rowBytes);


    // Set the image surface Cairo state to match our own. 
    _cairo_image_surface_set_repeat(surface->image, surface->base.repeat);
    _cairo_image_surface_set_matrix(surface->image,
                                    &(surface->base.matrix));

    *image_out = surface->image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_dest_image(void *abstract_surface,
                                         cairo_rectangle_t * interest_rect,
                                         cairo_image_surface_t **
                                         image_out,
                                         cairo_rectangle_t * image_rect,
                                         void **image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    image_rect->x = 0;
    image_rect->y = 0;
    image_rect->width = surface->image->width;
    image_rect->height = surface->image->height;

    *image_out = surface->image;

    return CAIRO_STATUS_SUCCESS;
}


static void
_cairo_quartz_surface_release_dest_image(void *abstract_surface,
                                         cairo_rectangle_t *
                                         intersect_rect,
                                         cairo_image_surface_t * image,
                                         cairo_rectangle_t * image_rect,
                                         void *image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    if (surface->image == image) {
        CGRect rect;

        rect = CGRectMake(0, 0, surface->width, surface->height);

        CGContextDrawImage(surface->context, rect, surface->cgImage);

	memset(surface->image->data, 0, surface->width * surface->height * 4);
    }
}

static cairo_int_status_t
_cairo_quartz_surface_set_clip_region(void *abstract_surface,
                                      pixman_region16_t * region)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_clip_region(surface->image, region);
}

static const struct _cairo_surface_backend cairo_quartz_surface_backend = {
    _cairo_quartz_surface_create_similar,
    _cairo_quartz_surface_finish,
    _cairo_quartz_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_quartz_surface_acquire_dest_image,
    _cairo_quartz_surface_release_dest_image,
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_quartz_surface_set_clip_region,
    NULL  /* show_glyphs */
};


cairo_surface_t *cairo_quartz_surface_create(CGContextRef context,
                                             int width, int height)
{
    cairo_quartz_surface_t *surface;

    surface = malloc(sizeof(cairo_quartz_surface_t));
    if (surface == NULL)
        return NULL;

    _cairo_surface_init(&surface->base, &cairo_quartz_surface_backend);

    surface->context = context;
    surface->width = width;
    surface->height = height;
    surface->image = NULL;
    surface->cgImage = NULL;

    // Set up the image surface which Cairo draws into and we blit to & from.
    void *foo;
    _cairo_quartz_surface_acquire_source_image(surface, &surface->image, &foo);

    return (cairo_surface_t *) surface;
}
