/*
 * $XFree86: $
 *
 * Copyright � 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#include "icint.h"

IcImage *
IcImageCreate (IcFormat	*format,
	       int	width,
	       int	height)
{
    IcImage	*image;
    IcPixels	*pixels;

    pixels = IcPixelsCreate (width, height, format->depth);
    if (pixels == NULL)
	return NULL;
    
    image = IcImageCreateForPixels (pixels, format);
    if (image == NULL) {
	IcPixelsDestroy (pixels);
	return NULL;
    }

    image->owns_pixels = 1;

    return image;
}

IcImage *
IcImageCreateForData (IcBits *data, IcFormat *format, int width, int height, int bpp, int stride)
{
    IcImage	*image;
    IcPixels	*pixels;

    pixels = IcPixelsCreateForData (data, width, height, format->depth, bpp, stride);
    if (pixels == NULL)
	return NULL;

    image = IcImageCreateForPixels (pixels, format);
    if (image == NULL) {
	IcPixelsDestroy (pixels);
	return NULL;
    }

    image->owns_pixels = 1;

    return image;
}

IcImage *
IcImageCreateForPixels (IcPixels	*pixels,
			IcFormat	*format)
{
    IcImage		*image;

    image = malloc (sizeof (IcImage));
    if (!image)
    {
	return NULL;
    }

    image->pixels = pixels;
    image->image_format = format;
    image->format_name = format->format_name;
/* XXX: What's all this about?
    if (pDrawable->type == DRAWABLE_PIXMAP)
    {
	++((PixmapPtr)pDrawable)->refcnt;
	image->pNext = 0;
    }
    else
    {
	image->pNext = GetPictureWindow(((WindowPtr) pDrawable));
	SetPictureWindow(((WindowPtr) pDrawable), image);
    }
*/

    IcImageInit (image);

    return image;
}

void
IcImageInit (IcImage *image)
{
    image->refcnt = 1;
    image->repeat = 0;
    image->graphicsExposures = FALSE;
    image->subWindowMode = ClipByChildren;
    image->polyEdge = PolyEdgeSharp;
    image->polyMode = PolyModePrecise;
    /* XXX: In the server this was FALSE. Why? */
    image->freeCompClip = TRUE;
    image->clientClipType = CT_NONE;
    image->componentAlpha = FALSE;

    image->alphaMap = 0;
    image->alphaOrigin.x = 0;
    image->alphaOrigin.y = 0;

    image->clipOrigin.x = 0;
    image->clipOrigin.y = 0;
    image->clientClip = 0;

    image->dither = None;

    image->stateChanges = (1 << (CPLastBit+1)) - 1;
/* XXX: What to lodge here?
    image->serialNumber = GC_CHANGE_SERIAL_BIT;
*/

    image->pCompositeClip = PixRegionCreate();
    PixRegionUnionRect (image->pCompositeClip, image->pCompositeClip,
			0, 0, image->pixels->width, image->pixels->height);

    image->transform = NULL;

    image->filter = IcFilterNearest;
    image->filter_params = 0;
    image->filter_nparams = 0;


    image->owns_pixels = 0;
}

int
IcImageSetTransform (IcImage		*image,
		     IcTransform	*transform)
{
    static const IcTransform	identity = { {
	{ xFixed1, 0x00000, 0x00000 },
	{ 0x00000, xFixed1, 0x00000 },
	{ 0x00000, 0x00000, xFixed1 },
    } };

    if (transform && memcmp (transform, &identity, sizeof (IcTransform)) == 0)
	transform = 0;
    
    if (transform)
    {
	if (!image->transform)
	{
	    image->transform = malloc (sizeof (IcTransform));
	    if (!image->transform)
		return BadAlloc;
	}
	*image->transform = *transform;
    }
    else
    {
	if (image->transform)
	{
	    free (image->transform);
	    image->transform = 0;
	}
    }
    return Success;
}

void
IcImageSetRepeat (IcImage	*image,
		  int		repeat)
{
    if (image)
	image->repeat = repeat;
}

void
IcImageSetFilter (IcImage	*image,
		  IcFilter	filter)
{
    if (image)
	image->filter = filter;
}

int
IcImageGetWidth (IcImage	*image)
{
    return image->pixels->width;
}

int
IcImageGetHeight (IcImage	*image)
{
    return image->pixels->height;
}

int
IcImageGetStride (IcImage	*image)
{
    return image->pixels->stride;
}

IcBits *
IcImageGetData (IcImage	*image)
{
    return image->pixels->data;
}

void
IcImageDestroy (IcImage *image)
{
    if (image->freeCompClip)
	PixRegionDestroy (image->pCompositeClip);

    if (image->owns_pixels)
	IcPixelsDestroy (image->pixels);

    free (image);
}

void
IcImageDestroyClip (IcImage *image)
{
    switch (image->clientClipType) {
    case CT_NONE:
	return;
    case CT_PIXMAP:
	IcImageDestroy (image->clientClip);
	break;
    default:
	PixRegionDestroy (image->clientClip);
	break;
    }
    image->clientClip = NULL;
    image->clientClipType = CT_NONE;
}    

int
IcImageSetClipRegion (IcImage	*image,
		      PixRegion	*region)
{
    IcImageDestroyClip (image);
    image->clientClip = region;
    image->clientClipType = CT_REGION;
    image->stateChanges |= CPClipMask;
    return Success;
}

#define BOUND(v)	(INT16) ((v) < MINSHORT ? MINSHORT : (v) > MAXSHORT ? MAXSHORT : (v))

static __inline Bool
IcClipImageReg (PixRegion	*region,
		PixRegion	*clip,
		int		dx,
		int		dy)
{
    if (PixRegionNumRects (region) == 1 &&
	PixRegionNumRects (clip) == 1)
    {
	PixRegionBox *pRbox = PixRegionRects (region);
	PixRegionBox *pCbox = PixRegionRects (clip);
	int	v;

	if (pRbox->x1 < (v = pCbox->x1 + dx))
	    pRbox->x1 = BOUND(v);
	if (pRbox->x2 > (v = pCbox->x2 + dx))
	    pRbox->x2 = BOUND(v);
	if (pRbox->y1 < (v = pCbox->y1 + dy))
	    pRbox->y1 = BOUND(v);
	if (pRbox->y2 > (v = pCbox->y2 + dy))
	    pRbox->y2 = BOUND(v);
	if (pRbox->x1 >= pRbox->x2 ||
	    pRbox->y1 >= pRbox->y2)
	{
	    PixRegionEmpty (region);
	}
    }
    else
    {
	PixRegionTranslate (region, dx, dy);
	PixRegionIntersect (region, clip, region);
	PixRegionTranslate (region, -dx, -dy);
    }
    return TRUE;
}
		  
static __inline Bool
IcClipImageSrc (PixRegion	*region,
		IcImage		*image,
		int		dx,
		int		dy)
{
    /* XXX what to do with clipping from transformed pictures? */
    if (image->transform)
	return TRUE;
    if (image->repeat)
    {
	if (image->clientClipType != CT_NONE)
	{
	    PixRegionTranslate (region, 
			   dx - image->clipOrigin.x,
			   dy - image->clipOrigin.y);
	    PixRegionIntersect (region, image->clientClip, region);
	    PixRegionTranslate (region, 
			   - (dx - image->clipOrigin.x),
			   - (dy - image->clipOrigin.y));
	}
	return TRUE;
    }
    else
    {
	return IcClipImageReg (region,
			       image->pCompositeClip,
			       dx,
			       dy);
    }
    return TRUE;
}

/* XXX: Need to decide what to do with this
#define NEXT_VAL(_type) (vlist ? (_type) *vlist++ : (_type) ulist++->val)

#define NEXT_PTR(_type) ((_type) ulist++->ptr)

int
IcImageChange (IcImage		*image,
	       Mask		vmask,
	       XID		*vlist,
	       DevUnion		*ulist,
	       int		*error_value)
{
    BITS32		index2;
    int			error = 0;
    BITS32		maskQ;
    
    maskQ = vmask;
    while (vmask && !error)
    {
	index2 = (BITS32) lowbit (vmask);
	vmask &= ~index2;
	image->stateChanges |= index2;
	switch (index2)
	{
	case CPRepeat:
	    {
		unsigned int	newr;
		newr = NEXT_VAL(unsigned int);
		if (newr <= xTrue)
		    image->repeat = newr;
		else
		{
		    *error_value = newr;
		    error = BadValue;
		}
	    }
	    break;
	case CPAlphaMap:
	    {
		IcImage *iAlpha;
		
		iAlpha = NEXT_PTR(IcImage *);
		if (iAlpha)
		    iAlpha->refcnt++;
		if (image->alphaMap)
		    IcImageDestroy ((pointer) image->alphaMap);
		image->alphaMap = iAlpha;
	    }
	    break;
	case CPAlphaXOrigin:
	    image->alphaOrigin.x = NEXT_VAL(INT16);
	    break;
	case CPAlphaYOrigin:
	    image->alphaOrigin.y = NEXT_VAL(INT16);
	    break;
	case CPClipXOrigin:
	    image->clipOrigin.x = NEXT_VAL(INT16);
	    break;
	case CPClipYOrigin:
	    image->clipOrigin.y = NEXT_VAL(INT16);
	    break;
	case CPClipMask:
	    {
		IcImage	    *mask;
		int	    clipType;

		mask = NEXT_PTR(IcImage *);
		if (mask) {
		    clipType = CT_PIXMAP;
		    mask->refcnt++;
		} else {
		    clipType = CT_NONE;
		}
		error = IcImageChangeClip (image, clipType,
					   (pointer)mask, 0);
		break;
	    }
	case CPGraphicsExposure:
	    {
		unsigned int	newe;
		newe = NEXT_VAL(unsigned int);
		if (newe <= xTrue)
		    image->graphicsExposures = newe;
		else
		{
		    *error_value = newe;
		    error = BadValue;
		}
	    }
	    break;
	case CPSubwindowMode:
	    {
		unsigned int	news;
		news = NEXT_VAL(unsigned int);
		if (news == ClipByChildren || news == IncludeInferiors)
		    image->subWindowMode = news;
		else
		{
		    *error_value = news;
		    error = BadValue;
		}
	    }
	    break;
	case CPPolyEdge:
	    {
		unsigned int	newe;
		newe = NEXT_VAL(unsigned int);
		if (newe == PolyEdgeSharp || newe == PolyEdgeSmooth)
		    image->polyEdge = newe;
		else
		{
		    *error_value = newe;
		    error = BadValue;
		}
	    }
	    break;
	case CPPolyMode:
	    {
		unsigned int	newm;
		newm = NEXT_VAL(unsigned int);
		if (newm == PolyModePrecise || newm == PolyModeImprecise)
		    image->polyMode = newm;
		else
		{
		    *error_value = newm;
		    error = BadValue;
		}
	    }
	    break;
	case CPDither:
	    image->dither = NEXT_VAL(Atom);
	    break;
	case CPComponentAlpha:
	    {
		unsigned int	newca;

		newca = NEXT_VAL (unsigned int);
		if (newca <= xTrue)
		    image->componentAlpha = newca;
		else
		{
		    *error_value = newca;
		    error = BadValue;
		}
	    }
	    break;
	default:
	    *error_value = maskQ;
	    error = BadValue;
	    break;
	}
    }
    return error;
}
*/

/* XXX: Do we need this?
int
SetPictureClipRects (PicturePtr	pPicture,
		     int	xOrigin,
		     int	yOrigin,
		     int	nRect,
		     xRectangle	*rects)
{
    ScreenPtr		pScreen = pPicture->pDrawable->pScreen;
    PictureScreenPtr	ps = GetPictureScreen(pScreen);
    PixRegion		*clientClip;
    int			result;

    clientClip = RECTS_TO_REGION(pScreen,
				 nRect, rects, CT_UNSORTED);
    if (!clientClip)
	return BadAlloc;
    result =(*ps->ChangePictureClip) (pPicture, CT_REGION, 
				      (pointer) clientClip, 0);
    if (result == Success)
    {
	pPicture->clipOrigin.x = xOrigin;
	pPicture->clipOrigin.y = yOrigin;
	pPicture->stateChanges |= CPClipXOrigin|CPClipYOrigin|CPClipMask;
	pPicture->serialNumber |= GC_CHANGE_SERIAL_BIT;
    }
    return result;
}
*/

Bool
IcComputeCompositeRegion (PixRegion	*region,
			  IcImage	*iSrc,
			  IcImage	*iMask,
			  IcImage	*iDst,
			  INT16		xSrc,
			  INT16		ySrc,
			  INT16		xMask,
			  INT16		yMask,
			  INT16		xDst,
			  INT16		yDst,
			  CARD16	width,
			  CARD16	height)
{
    int		v;
    int x1, y1, x2, y2;

    /* XXX: This code previously directly set the extents of the
       region here. I need to decide whether removing that has broken
       this. Also, it might be necessary to just make the PixRegion
       data structure transparent anyway in which case I can just put
       the code back. */
    x1 = xDst;
    v = xDst + width;
    x2 = BOUND(v);
    y1 = yDst;
    v = yDst + height;
    y2 = BOUND(v);
    /* Check for empty operation */
    if (x1 >= x2 ||
	y1 >= y2)
    {
	PixRegionEmpty (region);
	return TRUE;
    }
    /* clip against src */
    if (!IcClipImageSrc (region, iSrc, xDst - xSrc, yDst - ySrc))
    {
	PixRegionDestroy (region);
	return FALSE;
    }
    if (iSrc->alphaMap)
    {
	if (!IcClipImageSrc (region, iSrc->alphaMap,
			     xDst - (xSrc + iSrc->alphaOrigin.x),
			     yDst - (ySrc + iSrc->alphaOrigin.y)))
	{
	    PixRegionDestroy (region);
	    return FALSE;
	}
    }
    /* clip against mask */
    if (iMask)
    {
	if (!IcClipImageSrc (region, iMask, xDst - xMask, yDst - yMask))
	{
	    PixRegionDestroy (region);
	    return FALSE;
	}	
	if (iMask->alphaMap)
	{
	    if (!IcClipImageSrc (region, iMask->alphaMap,
				 xDst - (xMask + iMask->alphaOrigin.x),
				 yDst - (yMask + iMask->alphaOrigin.y)))
	    {
		PixRegionDestroy (region);
		return FALSE;
	    }
	}
    }
    if (!IcClipImageReg (region, iDst->pCompositeClip, 0, 0))
    {
	PixRegionDestroy (region);
	return FALSE;
    }
    if (iDst->alphaMap)
    {
	if (!IcClipImageReg (region, iDst->alphaMap->pCompositeClip,
			     -iDst->alphaOrigin.x,
			     -iDst->alphaOrigin.y))
	{
	    PixRegionDestroy (region);
	    return FALSE;
	}
    }
    return TRUE;
}

