/*
 * $Id: ictrap.c,v 1.22 2005-03-04 02:02:23 davidr Exp $
 *
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "icint.h"

pixman_image_t *
IcCreateAlphaPicture (pixman_image_t	*dst,
		      pixman_format_t	*format,
		      uint16_t	width,
		      uint16_t	height)
{
    pixman_image_t	*image;
    int own_format = 0;

    if (width > 32767 || height > 32767)
	return 0;

    if (!format)
    {
	own_format = 1;
	if (dst->polyEdge == PolyEdgeSharp)
	    format = pixman_format_create (PIXMAN_FORMAT_NAME_A1);
	else
	    format = pixman_format_create (PIXMAN_FORMAT_NAME_A8);
	if (!format)
	    return 0;
    }

    image = pixman_image_create (format, width, height); 

    if (own_format)
	pixman_format_destroy (format);

    /* XXX: Is this a reasonable way to clear the image? Would
       probably be preferable to use pixman_image_fill_rectangle once such a
       beast exists. */
    memset (image->pixels->data, 0, height * image->pixels->stride);

    return image;
}

static pixman_fixed16_16_t
pixman_line_fixed_x (const pixman_line_fixed_t *l, pixman_fixed16_16_t y, int ceil)
{
    pixman_fixed16_16_t    dx = l->p2.x - l->p1.x;
    xFixed_32_32    ex = (xFixed_32_32) (y - l->p1.y) * dx;
    pixman_fixed16_16_t    dy = l->p2.y - l->p1.y;
    if (ceil)
	ex += (dy - 1);
    return l->p1.x + (pixman_fixed16_16_t) (ex / dy);
}

static void
pixman_trapezoid_bounds (int ntrap, const pixman_trapezoid_t *traps, pixman_box16_t *box)
{
    box->y1 = MAXSHORT;
    box->y2 = MINSHORT;
    box->x1 = MAXSHORT;
    box->x2 = MINSHORT;
    for (; ntrap; ntrap--, traps++)
    {
	int16_t x1, y1, x2, y2;

	if (!xTrapezoidValid(traps))
	    continue;
	y1 = xFixedToInt (traps->top);
	if (y1 < box->y1)
	    box->y1 = y1;
	
	y2 = xFixedToInt (xFixedCeil (traps->bottom));
	if (y2 > box->y2)
	    box->y2 = y2;
	
	x1 = xFixedToInt (MIN (pixman_line_fixed_x (&traps->left, traps->top, 0),
			       pixman_line_fixed_x (&traps->left, traps->bottom, 0)));
	if (x1 < box->x1)
	    box->x1 = x1;
	
	x2 = xFixedToInt (xFixedCeil (MAX (pixman_line_fixed_x (&traps->right, traps->top, 1),
					   pixman_line_fixed_x (&traps->right, traps->bottom, 1))));
	if (x2 > box->x2)
	    box->x2 = x2;
    }
}

/* XXX: There are failure cases in this function. Don't we need to
 * propagate the errors out?
 */
void
pixman_composite_trapezoids (pixman_operator_t	      op,
			     pixman_image_t	      *src,
			     pixman_image_t	      *dst,
			     int		      xSrc,
			     int		      ySrc,
			     const pixman_trapezoid_t *traps,
			     int		      ntraps)
{
    pixman_image_t	*image = NULL;
    pixman_box16_t	bounds;
    int16_t		xDst, yDst;
    int16_t		xRel, yRel;
    pixman_format_t	*format;

    if (ntraps == 0)
	return;

    if (op == PIXMAN_OPERATOR_ADD && miIsSolidAlpha (src))
    {
	for (; ntraps; ntraps--, traps++)
	    fbRasterizeTrapezoid (dst, traps, 0, 0);
	return;
    }
    
    xDst = traps[0].left.p1.x >> 16;
    yDst = traps[0].left.p1.y >> 16;
    
    format = pixman_format_create (PIXMAN_FORMAT_NAME_A8);
    if (!format)
	return;

    pixman_trapezoid_bounds (ntraps, traps, &bounds);
    if (bounds.y1 >= bounds.y2 || bounds.x1 >= bounds.x2)
	return;
    image = IcCreateAlphaPicture (dst, format,
				  bounds.x2 - bounds.x1,
				  bounds.y2 - bounds.y1);
    if (!image)
	return;

    for (; ntraps; ntraps--, traps++)
    {
	if (!xTrapezoidValid(traps))
	    continue;
	fbRasterizeTrapezoid (image, traps, 
			      -bounds.x1, -bounds.y1);
    }

    xRel = bounds.x1 + xSrc - xDst;
    yRel = bounds.y1 + ySrc - yDst;
    pixman_composite (op, src, image, dst,
		      xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		      bounds.x2 - bounds.x1,
		      bounds.y2 - bounds.y1);
    pixman_image_destroy (image);

    pixman_format_destroy (format);
}

void
pixman_add_trapezoids (pixman_image_t		*dst,
		       int			x_off,
		       int			y_off,
		       const pixman_trapezoid_t	*traps,
		       int			ntraps)
{
    for (; ntraps; ntraps--, traps++)
    {
	if (!xTrapezoidValid (traps))
	    continue;

	fbRasterizeTrapezoid (dst, traps, x_off, y_off);
    }
}
