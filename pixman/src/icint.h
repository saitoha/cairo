/* $XFree86: $
 *
 * Copyright � 2003 Carl Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carl Worth not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Carl Worth makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * CARL WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL CARL WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ICINT_H_
#define _ICINT_H_

#include "ic.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <X11/X.h>
#include <X11/Xmd.h>

/* These few definitions avoid me needing to include servermd.h and misc.h from Xserver/include */
#ifndef BITMAP_SCANLINE_PAD
#define BITMAP_SCANLINE_PAD  32
#define LOG2_BITMAP_PAD		5
#define LOG2_BYTES_PER_SCANLINE_PAD	2
#endif

#define FALSE 0
#define TRUE  1

#define MAXSHORT SHRT_MAX
#define MINSHORT SHRT_MIN

/* XXX: What do we need from here?
#include "picture.h"
*/

#include "X11/Xprotostr.h"
#include "X11/extensions/Xrender.h"

#include "ic.h"

/* XXX: Most of this file is straight from fb.h and I imagine we can
   drop quite a bit of it. Once the real ic code starts to come
   together I can probably figure out what is not needed here. */

#define IC_UNIT	    (1 << IC_SHIFT)
#define IC_HALFUNIT (1 << (IC_SHIFT-1))
#define IC_MASK	    (IC_UNIT - 1)
#define IC_ALLONES  ((IcBits) -1)
    
/* whether to bother to include 24bpp support */
#ifndef ICNO24BIT
#define IC_24BIT
#endif

/*
 * Unless otherwise instructed, ic includes code to advertise 24bpp
 * windows with 32bpp image format for application compatibility
 */

#ifdef IC_24BIT
#ifndef ICNO24_32
#define IC_24_32BIT
#endif
#endif

#define IC_STIP_SHIFT	LOG2_BITMAP_PAD
#define IC_STIP_UNIT	(1 << IC_STIP_SHIFT)
#define IC_STIP_MASK	(IC_STIP_UNIT - 1)
#define IC_STIP_ALLONES	((IcStip) -1)
    
#define IC_STIP_ODDSTRIDE(s)	(((s) & (IC_MASK >> IC_STIP_SHIFT)) != 0)
#define IC_STIP_ODDPTR(p)	((((long) (p)) & (IC_MASK >> 3)) != 0)
    
#define IcStipStrideToBitsStride(s) (((s) >> (IC_SHIFT - IC_STIP_SHIFT)))
#define IcBitsStrideToStipStride(s) (((s) << (IC_SHIFT - IC_STIP_SHIFT)))
    
#define IcFullMask(n)   ((n) == IC_UNIT ? IC_ALLONES : ((((IcBits) 1) << n) - 1))


typedef uint32_t	    IcStip;
typedef int		    IcStride;


#ifdef IC_DEBUG
extern void IcValidateDrawable(DrawablePtr d);
extern void IcInitializeDrawable(DrawablePtr d);
extern void IcSetBits (IcStip *bits, int stride, IcStip data);
#define IC_HEAD_BITS   (IcStip) (0xbaadf00d)
#define IC_TAIL_BITS   (IcStip) (0xbaddf0ad)
#else
#define IcValidateDrawable(d)
#define fdInitializeDrawable(d)
#endif

#if BITMAP_BIT_ORDER == LSBFirst
#define IcScrLeft(x,n)	((x) >> (n))
#define IcScrRight(x,n)	((x) << (n))
/* #define IcLeftBits(x,n)	((x) & ((((IcBits) 1) << (n)) - 1)) */
#define IcLeftStipBits(x,n) ((x) & ((((IcStip) 1) << (n)) - 1))
#define IcStipMoveLsb(x,s,n)	(IcStipRight (x,(s)-(n)))
#define IcPatternOffsetBits	0
#else
#define IcScrLeft(x,n)	((x) << (n))
#define IcScrRight(x,n)	((x) >> (n))
/* #define IcLeftBits(x,n)	((x) >> (IC_UNIT - (n))) */
#define IcLeftStipBits(x,n) ((x) >> (IC_STIP_UNIT - (n)))
#define IcStipMoveLsb(x,s,n)	(x)
#define IcPatternOffsetBits	(sizeof (IcBits) - 1)
#endif

#define IcStipLeft(x,n)	IcScrLeft(x,n)
#define IcStipRight(x,n) IcScrRight(x,n)

#define IcRotLeft(x,n)	IcScrLeft(x,n) | (n ? IcScrRight(x,IC_UNIT-n) : 0)
#define IcRotRight(x,n)	IcScrRight(x,n) | (n ? IcScrLeft(x,IC_UNIT-n) : 0)

#define IcRotStipLeft(x,n)  IcStipLeft(x,n) | (n ? IcStipRight(x,IC_STIP_UNIT-n) : 0)
#define IcRotStipRight(x,n)  IcStipRight(x,n) | (n ? IcStipLeft(x,IC_STIP_UNIT-n) : 0)

#define IcLeftMask(x)	    ( ((x) & IC_MASK) ? \
			     IcScrRight(IC_ALLONES,(x) & IC_MASK) : 0)
#define IcRightMask(x)	    ( ((IC_UNIT - (x)) & IC_MASK) ? \
			     IcScrLeft(IC_ALLONES,(IC_UNIT - (x)) & IC_MASK) : 0)

#define IcLeftStipMask(x)   ( ((x) & IC_STIP_MASK) ? \
			     IcStipRight(IC_STIP_ALLONES,(x) & IC_STIP_MASK) : 0)
#define IcRightStipMask(x)  ( ((IC_STIP_UNIT - (x)) & IC_STIP_MASK) ? \
			     IcScrLeft(IC_STIP_ALLONES,(IC_STIP_UNIT - (x)) & IC_STIP_MASK) : 0)

#define IcBitsMask(x,w)	(IcScrRight(IC_ALLONES,(x) & IC_MASK) & \
			 IcScrLeft(IC_ALLONES,(IC_UNIT - ((x) + (w))) & IC_MASK))

#define IcStipMask(x,w)	(IcStipRight(IC_STIP_ALLONES,(x) & IC_STIP_MASK) & \
			 IcStipLeft(IC_STIP_ALLONES,(IC_STIP_UNIT - ((x)+(w))) & IC_STIP_MASK))


#define IcMaskBits(x,w,l,n,r) { \
    n = (w); \
    r = IcRightMask((x)+n); \
    l = IcLeftMask(x); \
    if (l) { \
	n -= IC_UNIT - ((x) & IC_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= IC_SHIFT; \
}

#ifdef ICNOPIXADDR
#define IcMaskBitsBytes(x,w,copy,l,lb,n,r,rb) IcMaskBits(x,w,l,n,r)
#define IcDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    *dst = IcDoMaskRRop(*dst,and,xor,l); \
}
#define IcDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    *dst = IcDoMaskRRop(*dst,and,xor,r); \
}
#else

#define IcByteMaskInvalid   0x10

#define IcPatternOffset(o,t)  ((o) ^ (IcPatternOffsetBits & ~(sizeof (t) - 1)))

#define IcPtrOffset(p,o,t)		((t *) ((CARD8 *) (p) + (o)))
#define IcSelectPatternPart(xor,o,t)	((xor) >> (IcPatternOffset (o,t) << 3))
#define IcStorePart(dst,off,t,xor)	(*IcPtrOffset(dst,off,t) = \
					 IcSelectPart(xor,off,t))
#ifndef IcSelectPart
#define IcSelectPart(x,o,t) IcSelectPatternPart(x,o,t)
#endif

#define IcMaskBitsBytes(x,w,copy,l,lb,n,r,rb) { \
    n = (w); \
    lb = 0; \
    rb = 0; \
    r = IcRightMask((x)+n); \
    if (r) { \
	/* compute right byte length */ \
	if ((copy) && (((x) + n) & 7) == 0) { \
	    rb = (((x) + n) & IC_MASK) >> 3; \
	} else { \
	    rb = IcByteMaskInvalid; \
	} \
    } \
    l = IcLeftMask(x); \
    if (l) { \
	/* compute left byte length */ \
	if ((copy) && ((x) & 7) == 0) { \
	    lb = ((x) & IC_MASK) >> 3; \
	} else { \
	    lb = IcByteMaskInvalid; \
	} \
	/* subtract out the portion painted by leftMask */ \
	n -= IC_UNIT - ((x) & IC_MASK); \
	if (n < 0) { \
	    if (lb != IcByteMaskInvalid) { \
		if (rb == IcByteMaskInvalid) { \
		    lb = IcByteMaskInvalid; \
		} else if (rb) { \
		    lb |= (rb - lb) << (IC_SHIFT - 3); \
		    rb = 0; \
		} \
	    } \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	}\
    } \
    n >>= IC_SHIFT; \
}

#if IC_SHIFT == 6
#define IcDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (IcBits) - 7) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (5 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (6 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (5 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 6): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 5): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (IcBits) - 4) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 4) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 4) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 4): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break;

#define IcDoRightMaskByteRRop6Cases(dst,xor) \
    case 4: \
	IcStorePart(dst,0,CARD32,xor); \
	break; \
    case 5: \
	IcStorePart(dst,0,CARD32,xor); \
	IcStorePart(dst,4,CARD8,xor); \
	break; \
    case 6: \
	IcStorePart(dst,0,CARD32,xor); \
	IcStorePart(dst,4,CARD16,xor); \
	break; \
    case 7: \
	IcStorePart(dst,0,CARD32,xor); \
	IcStorePart(dst,4,CARD16,xor); \
	IcStorePart(dst,6,CARD8,xor); \
	break;
#else
#define IcDoLeftMaskByteRRop6Cases(dst,xor)
#define IcDoRightMaskByteRRop6Cases(dst,xor)
#endif

#define IcDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    switch (lb) { \
    IcDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (IcBits) - 3) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 3,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 3) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 3,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 2) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case sizeof (IcBits) - 3: \
	IcStorePart(dst,sizeof (IcBits) - 3,CARD8,xor); \
    case sizeof (IcBits) - 2: \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD16,xor); \
	break; \
    case sizeof (IcBits) - 1: \
	IcStorePart(dst,sizeof (IcBits) - 1,CARD8,xor); \
	break; \
    default: \
	*dst = IcDoMaskRRop(*dst, and, xor, l); \
	break; \
    } \
}


#define IcDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    switch (rb) { \
    case 1: \
	IcStorePart(dst,0,CARD8,xor); \
	break; \
    case 2: \
	IcStorePart(dst,0,CARD16,xor); \
	break; \
    case 3: \
	IcStorePart(dst,0,CARD16,xor); \
	IcStorePart(dst,2,CARD8,xor); \
	break; \
    IcDoRightMaskByteRRop6Cases(dst,xor) \
    default: \
	*dst = IcDoMaskRRop (*dst, and, xor, r); \
    } \
}
#endif

#define IcMaskStip(x,w,l,n,r) { \
    n = (w); \
    r = IcRightStipMask((x)+n); \
    l = IcLeftStipMask(x); \
    if (l) { \
	n -= IC_STIP_UNIT - ((x) & IC_STIP_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= IC_STIP_SHIFT; \
}

/*
 * These macros are used to transparently stipple
 * in copy mode; the expected usage is with 'n' constant
 * so all of the conditional parts collapse into a minimal
 * sequence of partial word writes
 *
 * 'n' is the bytemask of which bytes to store, 'a' is the address
 * of the IcBits base unit, 'o' is the offset within that unit
 *
 * The term "lane" comes from the hardware term "byte-lane" which
 */

#define IcLaneCase1(n,a,o)  ((n) == 0x01 ? \
			     (*(CARD8 *) ((a)+IcPatternOffset(o,CARD8)) = \
			      fgxor) : 0)
#define IcLaneCase2(n,a,o)  ((n) == 0x03 ? \
			     (*(CARD16 *) ((a)+IcPatternOffset(o,CARD16)) = \
			      fgxor) : \
			     ((void)IcLaneCase1((n)&1,a,o), \
				    IcLaneCase1((n)>>1,a,(o)+1)))
#define IcLaneCase4(n,a,o)  ((n) == 0x0f ? \
			     (*(CARD32 *) ((a)+IcPatternOffset(o,CARD32)) = \
			      fgxor) : \
			     ((void)IcLaneCase2((n)&3,a,o), \
				    IcLaneCase2((n)>>2,a,(o)+2)))
#define IcLaneCase8(n,a,o)  ((n) == 0x0ff ? (*(IcBits *) ((a)+(o)) = fgxor) : \
			     ((void)IcLaneCase4((n)&15,a,o), \
				    IcLaneCase4((n)>>4,a,(o)+4)))

#if IC_SHIFT == 6
#define IcLaneCase(n,a)   IcLaneCase8(n,(CARD8 *) (a),0)
#endif

#if IC_SHIFT == 5
#define IcLaneCase(n,a)   IcLaneCase4(n,(CARD8 *) (a),0)
#endif

/* Rotate a filled pixel value to the specified alignement */
#define IcRot24(p,b)	    (IcScrRight(p,b) | IcScrLeft(p,24-(b)))
#define IcRot24Stip(p,b)    (IcStipRight(p,b) | IcStipLeft(p,24-(b)))

/* step a filled pixel value to the next/previous IC_UNIT alignment */
#define IcNext24Pix(p)	(IcRot24(p,(24-IC_UNIT%24)))
#define IcPrev24Pix(p)	(IcRot24(p,IC_UNIT%24))
#define IcNext24Stip(p)	(IcRot24(p,(24-IC_STIP_UNIT%24)))
#define IcPrev24Stip(p)	(IcRot24(p,IC_STIP_UNIT%24))

/* step a rotation value to the next/previous rotation value */
#if IC_UNIT == 64
#define IcNext24Rot(r)        ((r) == 16 ? 0 : (r) + 8)
#define IcPrev24Rot(r)        ((r) == 0 ? 16 : (r) - 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define IcFirst24Rot(x)		(((x) + 8) % 24)
#else
#define IcFirst24Rot(x)		((x) % 24)
#endif

#endif

#if IC_UNIT == 32
#define IcNext24Rot(r)        ((r) == 0 ? 16 : (r) - 8)
#define IcPrev24Rot(r)        ((r) == 16 ? 0 : (r) + 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define IcFirst24Rot(x)		(((x) + 16) % 24)
#else
#define IcFirst24Rot(x)		((x) % 24)
#endif
#endif

#define IcNext24RotStip(r)        ((r) == 0 ? 16 : (r) - 8)
#define IcPrev24RotStip(r)        ((r) == 16 ? 0 : (r) + 8)

/* Whether 24-bit specific code is needed for this filled pixel value */
#define IcCheck24Pix(p)	((p) == IcNext24Pix(p))

#define IcGetPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = icpixels->data; \
    (_stride_) = icpixels->stride / sizeof(IcBits); \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
    (yoff) = icpixels->y; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
}

#define IcGetStipPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = (IcStip *) icpixels->data; \
    (_stride_) = icpixels->stride; \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; \
    (yoff) = icpixels->y; \
}

/*
 * XFree86 empties the root BorderClip when the VT is inactive,
 * here's a macro which uses that to disable GetImage and GetSpans
 */

#define IcWindowEnabled(pWin) \
    REGION_NOTEMPTY((pWin)->drawable.pScreen, \
		    &WindowTable[(pWin)->drawable.pScreen->myNum]->borderClip)

#define IcDrawableEnabled(pDrawable) \
    ((pDrawable)->type == DRAWABLE_PIXMAP ? \
     TRUE : IcWindowEnabled((WindowPtr) pDrawable))

#ifdef IC_OLD_SCREEN
#define BitsPerPixel(d) (\
    ((1 << PixmapWidthPaddingInfo[d].padBytesLog2) * 8 / \
    (PixmapWidthPaddingInfo[d].padRoundUp+1)))
#endif

#define IcPowerOfTwo(w)	    (((w) & ((w) - 1)) == 0)
/*
 * Accelerated tiles are power of 2 width <= IC_UNIT
 */
#define IcEvenTile(w)	    ((w) <= IC_UNIT && IcPowerOfTwo(w))
/*
 * Accelerated stipples are power of 2 width and <= IC_UNIT/dstBpp
 * with dstBpp a power of 2 as well
 */
#define IcEvenStip(w,bpp)   ((w) * (bpp) <= IC_UNIT && IcPowerOfTwo(w) && IcPowerOfTwo(bpp))

/*
 * icblt.c
 */
void
IcBlt (IcBits   *src, 
       IcStride	srcStride,
       int	srcX,
       
       IcBits   *dst,
       IcStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       IcBits	pm,
       int	bpp,
       
       Bool	reverse,
       Bool	upsidedown);

void
IcBlt24 (IcBits	    *srcLine,
	 IcStride   srcStride,
	 int	    srcX,

	 IcBits	    *dstLine,
	 IcStride   dstStride,
	 int	    dstX,

	 int	    width, 
	 int	    height,

	 int	    alu,
	 IcBits	    pm,

	 Bool	    reverse,
	 Bool	    upsidedown);
    
void
IcBltStip (IcStip   *src,
	   IcStride srcStride,	    /* in IcStip units, not IcBits units */
	   int	    srcX,
	   
	   IcStip   *dst,
	   IcStride dstStride,	    /* in IcStip units, not IcBits units */
	   int	    dstX,

	   int	    width, 
	   int	    height,

	   int	    alu,
	   IcBits   pm,
	   int	    bpp);
    
/*
 * icbltone.c
 */
void
IcBltOne (IcStip   *src,
	  IcStride srcStride,
	  int	   srcX,
	  IcBits   *dst,
	  IcStride dstStride,
	  int	   dstX,
	  int	   dstBpp,

	  int	   width,
	  int	   height,

	  IcBits   fgand,
	  IcBits   icxor,
	  IcBits   bgand,
	  IcBits   bgxor);
 
#ifdef IC_24BIT
void
IcBltOne24 (IcStip    *src,
	  IcStride  srcStride,	    /* IcStip units per scanline */
	  int	    srcX,	    /* bit position of source */
	  IcBits    *dst,
	  IcStride  dstStride,	    /* IcBits units per scanline */
	  int	    dstX,	    /* bit position of dest */
	  int	    dstBpp,	    /* bits per destination unit */

	  int	    width,	    /* width in bits of destination */
	  int	    height,	    /* height in scanlines */

	  IcBits    fgand,	    /* rrop values */
	  IcBits    fgxor,
	  IcBits    bgand,
	  IcBits    bgxor);
#endif

/*
 * icstipple.c
 */

void
IcTransparentSpan (IcBits   *dst,
		   IcBits   stip,
		   IcBits   fgxor,
		   int	    n);

void
IcEvenStipple (IcBits   *dst,
	       IcStride dstStride,
	       int	dstX,
	       int	dstBpp,

	       int	width,
	       int	height,

	       IcStip   *stip,
	       IcStride	stipStride,
	       int	stipHeight,

	       IcBits   fgand,
	       IcBits   fgxor,
	       IcBits   bgand,
	       IcBits   bgxor,

	       int	xRot,
	       int	yRot);

void
IcOddStipple (IcBits	*dst,
	      IcStride	dstStride,
	      int	dstX,
	      int	dstBpp,

	      int	width,
	      int	height,

	      IcStip	*stip,
	      IcStride	stipStride,
	      int	stipWidth,
	      int	stipHeight,

	      IcBits	fgand,
	      IcBits	fgxor,
	      IcBits	bgand,
	      IcBits	bgxor,

	      int	xRot,
	      int	yRot);

void
IcStipple (IcBits   *dst,
	   IcStride dstStride,
	   int	    dstX,
	   int	    dstBpp,

	   int	    width,
	   int	    height,

	   IcStip   *stip,
	   IcStride stipStride,
	   int	    stipWidth,
	   int	    stipHeight,
	   Bool	    even,

	   IcBits   fgand,
	   IcBits   fgxor,
	   IcBits   bgand,
	   IcBits   bgxor,

	   int	    xRot,
	   int	    yRot);

typedef struct _IcPixels {
    IcBits		*data;
    unsigned int	width;
    unsigned int	height;
    unsigned int	depth;
    unsigned int	bpp;
    unsigned int	stride;
    int			x;
    int			y;
    unsigned int	refcnt;
} IcPixels;

/* XXX: This is to avoid including colormap.h from the server includes */
typedef CARD32 Pixel;

/* icutil.c */
IcBits
IcReplicatePixel (Pixel p, int bpp);

/* XXX: This is to avoid including gc.h from the server includes */
/* clientClipType field in GC */
#define CT_NONE			0
#define CT_PIXMAP		1
#define CT_REGION		2
#define CT_UNSORTED		6
#define CT_YSORTED		10
#define CT_YXSORTED		14
#define CT_YXBANDED		18

#include "icimage.h"

/* icformat.c */

IcFormat *
_IcFormatCreate (IcFormatName name);

void
_IcFormatDestroy (IcFormat *format);

/* icimage.c */

IcImage *
IcImageCreateForPixels (IcPixels	*pixels,
			IcFormat	*format);

/* icpixels.c */

IcPixels *
IcPixelsCreate (int width, int height, int depth);

IcPixels *
IcPixelsCreateForData (IcBits *data, int width, int height, int depth, int bpp, int stride);

void
IcPixelsDestroy (IcPixels *pixels);

/* ictrap.c */

void
IcRasterizeTrapezoid (IcImage		*pMask,
		      const IcTrapezoid  *pTrap,
		      int		x_off,
		      int		y_off);


#include "icrop.h"

/* XXX: For now, I'm just wholesale pasting Xserver/render/picture.h here: */
#ifndef _PICTURE_H_
#define _PICTURE_H_

typedef struct _DirectFormat	*DirectFormatPtr;
typedef struct _PictFormat	*PictFormatPtr;
typedef struct _Picture		*PicturePtr;

/*
 * While the protocol is generous in format support, the
 * sample implementation allows only packed RGB and GBR
 * representations for data to simplify software rendering,
 */
#define PICT_FORMAT(bpp,type,a,r,g,b)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((a) << 12) | \
					 ((r) << 8) | \
					 ((g) << 4) | \
					 ((b)))

/*
 * gray/color formats use a visual index instead of argb
 */
#define PICT_VISFORMAT(bpp,type,vi)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((vi)))

#define PICT_FORMAT_BPP(f)	(((f) >> 24)       )
#define PICT_FORMAT_TYPE(f)	(((f) >> 16) & 0xff)
#define PICT_FORMAT_A(f)	(((f) >> 12) & 0x0f)
#define PICT_FORMAT_R(f)	(((f) >>  8) & 0x0f)
#define PICT_FORMAT_G(f)	(((f) >>  4) & 0x0f)
#define PICT_FORMAT_B(f)	(((f)      ) & 0x0f)
#define PICT_FORMAT_RGB(f)	(((f)      ) & 0xfff)
#define PICT_FORMAT_VIS(f)	(((f)      ) & 0xffff)

#define PICT_TYPE_OTHER	0
#define PICT_TYPE_A	1
#define PICT_TYPE_ARGB	2
#define PICT_TYPE_ABGR	3
#define PICT_TYPE_COLOR	4
#define PICT_TYPE_GRAY	5

#define PICT_FORMAT_COLOR(f)	(PICT_FORMAT_TYPE(f) & 2)

/* 32bpp formats */
#define PICT_a8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,8,8,8,8)
#define PICT_x8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_a8b8g8r8	PICT_FORMAT(32,PICT_TYPE_ABGR,8,8,8,8)
#define PICT_x8b8g8r8	PICT_FORMAT(32,PICT_TYPE_ABGR,0,8,8,8)

/* 24bpp formats */
#define PICT_r8g8b8	PICT_FORMAT(24,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_b8g8r8	PICT_FORMAT(24,PICT_TYPE_ABGR,0,8,8,8)

/* 16bpp formats */
#define PICT_r5g6b5	PICT_FORMAT(16,PICT_TYPE_ARGB,0,5,6,5)
#define PICT_b5g6r5	PICT_FORMAT(16,PICT_TYPE_ABGR,0,5,6,5)

#define PICT_a1r5g5b5	PICT_FORMAT(16,PICT_TYPE_ARGB,1,5,5,5)
#define PICT_x1r5g5b5	PICT_FORMAT(16,PICT_TYPE_ARGB,0,5,5,5)
#define PICT_a1b5g5r5	PICT_FORMAT(16,PICT_TYPE_ABGR,1,5,5,5)
#define PICT_x1b5g5r5	PICT_FORMAT(16,PICT_TYPE_ABGR,0,5,5,5)
#define PICT_a4r4g4b4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_x4r4g4b4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_a4b4g4r4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_x4b4g4r4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)

/* 8bpp formats */
#define PICT_a8		PICT_FORMAT(8,PICT_TYPE_A,8,0,0,0)
#define PICT_r3g3b2	PICT_FORMAT(8,PICT_TYPE_ARGB,0,3,3,2)
#define PICT_b2g3r3	PICT_FORMAT(8,PICT_TYPE_ABGR,0,3,3,2)
#define PICT_a2r2g2b2	PICT_FORMAT(8,PICT_TYPE_ARGB,2,2,2,2)
#define PICT_a2b2g2r2	PICT_FORMAT(8,PICT_TYPE_ABGR,2,2,2,2)

#define PICT_c8		PICT_FORMAT(8,PICT_TYPE_COLOR,0,0,0,0)
#define PICT_g8		PICT_FORMAT(8,PICT_TYPE_GRAY,0,0,0,0)

/* 4bpp formats */
#define PICT_a4		PICT_FORMAT(4,PICT_TYPE_A,4,0,0,0)
#define PICT_r1g2b1	PICT_FORMAT(4,PICT_TYPE_ARGB,0,1,2,1)
#define PICT_b1g2r1	PICT_FORMAT(4,PICT_TYPE_ABGR,0,1,2,1)
#define PICT_a1r1g1b1	PICT_FORMAT(4,PICT_TYPE_ARGB,1,1,1,1)
#define PICT_a1b1g1r1	PICT_FORMAT(4,PICT_TYPE_ABGR,1,1,1,1)
				    
#define PICT_c4		PICT_FORMAT(4,PICT_TYPE_COLOR,0,0,0,0)
#define PICT_g4		PICT_FORMAT(4,PICT_TYPE_GRAY,0,0,0,0)

/* 1bpp formats */
#define PICT_a1		PICT_FORMAT(1,PICT_TYPE_A,1,0,0,0)

#define PICT_g1		PICT_FORMAT(1,PICT_TYPE_GRAY,0,0,0,0)

/*
 * For dynamic indexed visuals (GrayScale and PseudoColor), these control the 
 * selection of colors allocated for drawing to Pictures.  The default
 * policy depends on the size of the colormap:
 *
 * Size		Default Policy
 * ----------------------------
 *  < 64	PolicyMono
 *  < 256	PolicyGray
 *  256		PolicyColor (only on PseudoColor)
 *
 * The actual allocation code lives in miindex.c, and so is
 * austensibly server dependent, but that code does:
 *
 * PolicyMono	    Allocate no additional colors, use black and white
 * PolicyGray	    Allocate 13 gray levels (11 cells used)
 * PolicyColor	    Allocate a 4x4x4 cube and 13 gray levels (71 cells used)
 * PolicyAll	    Allocate as big a cube as possible, fill with gray (all)
 *
 * Here's a picture to help understand how many colors are
 * actually allocated (this is just the gray ramp):
 *
 *                 gray level
 * all   0000 1555 2aaa 4000 5555 6aaa 8000 9555 aaaa bfff d555 eaaa ffff
 * b/w   0000                                                        ffff
 * 4x4x4                     5555                aaaa
 * extra      1555 2aaa 4000      6aaa 8000 9555      bfff d555 eaaa
 *
 * The default colormap supplies two gray levels (black/white), the
 * 4x4x4 cube allocates another two and nine more are allocated to fill
 * in the 13 levels.  When the 4x4x4 cube is not allocated, a total of
 * 11 cells are allocated.
 */   

#define PictureCmapPolicyInvalid    -1
#define PictureCmapPolicyDefault    0
#define PictureCmapPolicyMono	    1
#define PictureCmapPolicyGray	    2
#define PictureCmapPolicyColor	    3
#define PictureCmapPolicyAll	    4

extern int  PictureCmapPolicy;

int	PictureParseCmapPolicy (const char *name);

/* Fixed point updates from Carl Worth, USC, Information Sciences Institute */

#ifdef WIN32
typedef __int64		xFixed_32_32;
#else
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
typedef long		xFixed_32_32;
# else
#  if defined(__GNUC__) && \
    ((__GNUC__ > 2) || \
     ((__GNUC__ == 2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ > 7)))
__extension__
#  endif
typedef long long int	xFixed_32_32;
# endif
#endif

typedef CARD32		xFixed_1_31;
typedef CARD32		xFixed_1_16;
typedef INT32		xFixed_16_16;

/*
 * An unadorned "xFixed" is the same as xFixed_16_16, 
 * (since it's quite common in the code) 
 */
typedef	xFixed_16_16	xFixed;
#define XFIXED_BITS	16

#define xFixedToInt(f)	(int) ((f) >> XFIXED_BITS)
#define IntToxFixed(i)	((xFixed) (i) << XFIXED_BITS)
#define xFixedE		((xFixed) 1)
#define xFixed1		(IntToxFixed(1))
#define xFixed1MinusE	(xFixed1 - xFixedE)
#define xFixedFrac(f)	((f) & xFixed1MinusE)
#define xFixedFloor(f)	((f) & ~xFixed1MinusE)
#define xFixedCeil(f)	xFixedFloor((f) + xFixed1MinusE)

#define xFixedFraction(f)	((f) & xFixed1MinusE)
#define xFixedMod2(f)		((f) & (xFixed1 | xFixed1MinusE))

/* whether 't' is a well defined not obviously empty trapezoid */
#define xTrapezoidValid(t)  ((t)->left.p1.y != (t)->left.p2.y && \
			     (t)->right.p1.y != (t)->right.p2.y && \
			     (int) ((t)->bottom - (t)->top) > 0)

/*
 * Standard NTSC luminance conversions:
 *
 *  y = r * 0.299 + g * 0.587 + b * 0.114
 *
 * Approximate this for a bit more speed:
 *
 *  y = (r * 153 + g * 301 + b * 58) / 512
 *
 * This gives 17 bits of luminance; to get 15 bits, lop the low two
 */

#define CvtR8G8B8toY15(s)	(((((s) >> 16) & 0xff) * 153 + \
				  (((s) >>  8) & 0xff) * 301 + \
				  (((s)      ) & 0xff) * 58) >> 2)

#endif /* _PICTURE_H_ */

#endif
