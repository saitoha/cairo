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

#ifndef CAIRO_QUARTZ_PRIVATE_H
#define CAIRO_QUARTZ_PRIVATE_H

#include <cairoint.h>
#include <cairo-quartz.h>

#ifdef CAIRO_NQUARTZ_SUPPORT_AGL
#include <AGL/agl.h>
#include <OpenGL/gl.h>

typedef AGLContext nquartz_agl_context_type;
#else
typedef void* nquartz_agl_context_type;
#endif

typedef struct cairo_nquartz_surface {
    cairo_surface_t base;

    void *imageData;

    CGContextRef cgContext;
    CGAffineTransform cgContextBaseCTM;

    cairo_rectangle_int16_t extents;

    /* These are stored while drawing operations are in place, set up
     * by nquartz_setup_source() and nquartz_finish_source()
     */
    CGAffineTransform imageTransform;
    CGImageRef sourceImage;
    CGShadingRef sourceShading;
    CGPatternRef sourcePattern;
    nquartz_agl_context_type aglContext;
} cairo_nquartz_surface_t, cairo_quartz_surface_t;

cairo_bool_t
_cairo_scaled_font_is_atsui (cairo_scaled_font_t *sfont);

ATSUStyle
_cairo_atsui_scaled_font_get_atsu_style (cairo_scaled_font_t *sfont);

ATSUFontID
_cairo_atsui_scaled_font_get_atsu_font_id (cairo_scaled_font_t *sfont);

#endif /* CAIRO_QUARTZ_PRIVATE_H */
