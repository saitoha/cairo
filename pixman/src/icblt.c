/*
 * Id: $
 *
 * Copyright © 1998 Keith Packard
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

#include "pixman-xserver-compat.h"

#define InitializeShifts(sx,dx,ls,rs) { \
    if (sx != dx) { \
	if (sx > dx) { \
	    ls = sx - dx; \
	    rs = FB_UNIT - ls; \
	} else { \
	    rs = dx - sx; \
	    ls = FB_UNIT - rs; \
	} \
    } \
}

void
IcBlt (FbBits   *srcLine,
       IcStride	srcStride,
       int	srcX,
       
       FbBits   *dstLine,
       IcStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       FbBits	pm,
       int	bpp,
       
       int	reverse,
       int	upsidedown)
{
    FbBits  *src, *dst;
    int	    leftShift, rightShift;
    FbBits  startmask, endmask;
    FbBits  bits, bits1;
    int	    n, nmiddle;
    int    destInvarient;
    int	    startbyte, endbyte;
    IcDeclareMergeRop ();
 
    /* are we just copying multiples of 8 bits?  if so, run, forrest, run!
       the memcpy()'s should be pluggable ala mplayer|xine - perhaps we can get
       one of the above to give up their code for us.
     */
    if((pm==FB_ALLONES) && (alu==GXcopy) && !reverse && (srcX&7)==0 && (dstX&7)==0 && (width&7)==0)
    {
		uint8_t *isrc=(uint8_t *)srcLine;
		uint8_t *idst=(uint8_t *)dstLine;
		int sstride=srcStride*sizeof(FbBits);
		int dstride=dstStride*sizeof(FbBits);
		int j;
		width>>=3;
		isrc+=(srcX>>3);
		idst+=(dstX>>3);
		if(!upsidedown)
			for(j=0;j<height;j++)
				memcpy(idst+j*dstride, isrc+j*sstride, width);
		else
			for(j=(height-1);j>=0;j--)
				memcpy(idst+j*dstride, isrc+j*sstride, width);
	
		return;
    }
    
#ifdef FB_24BIT
    if (bpp == 24 && !IcCheck24Pix (pm))
    {
	IcBlt24 (srcLine, srcStride, srcX, dstLine, dstStride, dstX,
		 width, height, alu, pm, reverse, upsidedown);
	return;
    }
#endif
    IcInitializeMergeRop(alu, pm);
    destInvarient = IcDestInvarientMergeRop();
    if (upsidedown)
    {
	srcLine += (height - 1) * (srcStride);
	dstLine += (height - 1) * (dstStride);
	srcStride = -srcStride;
	dstStride = -dstStride;
    }
    IcMaskBitsBytes (dstX, width, destInvarient, startmask, startbyte,
		     nmiddle, endmask, endbyte);
    if (reverse)
    {
	srcLine += ((srcX + width - 1) >> FB_SHIFT) + 1;
	dstLine += ((dstX + width - 1) >> FB_SHIFT) + 1;
	srcX = (srcX + width - 1) & FB_MASK;
	dstX = (dstX + width - 1) & FB_MASK;
    }
    else
    {
	srcLine += srcX >> FB_SHIFT;
	dstLine += dstX >> FB_SHIFT;
	srcX &= FB_MASK;
	dstX &= FB_MASK;
    }
    if (srcX == dstX)
    {
	while (height--)
	{
	    src = srcLine;
	    srcLine += srcStride;
	    dst = dstLine;
	    dstLine += dstStride;
	    if (reverse)
	    {
		if (endmask)
		{
		    bits = *--src;
		    --dst;
		    IcDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
			*--dst = IcDoDestInvarientMergeRop(*--src);
		}
		else
		{
		    while (n--)
		    {
			bits = *--src;
			--dst;
			*dst = IcDoMergeRop (bits, *dst);
		    }
		}
		if (startmask)
		{
		    bits = *--src;
		    --dst;
		    IcDoLeftMaskByteMergeRop(dst, bits, startbyte, startmask);
		}
	    }
	    else
	    {
		if (startmask)
		{
		    bits = *src++;
		    IcDoLeftMaskByteMergeRop(dst, bits, startbyte, startmask);
		    dst++;
		}
		n = nmiddle;
		if (destInvarient)
		{
#if 0
		    /*
		     * This provides some speedup on screen->screen blts
		     * over the PCI bus, usually about 10%.  But Ic
		     * isn't usually used for this operation...
		     */
		    if (_ca2 + 1 == 0 && _cx2 == 0)
		    {
			FbBits	t1, t2, t3, t4;
			while (n >= 4)
			{
			    t1 = *src++;
			    t2 = *src++;
			    t3 = *src++;
			    t4 = *src++;
			    *dst++ = t1;
			    *dst++ = t2;
			    *dst++ = t3;
			    *dst++ = t4;
			    n -= 4;
			}
		    }
#endif
		    while (n--)
			*dst++ = IcDoDestInvarientMergeRop(*src++);
		}
		else
		{
		    while (n--)
		    {
			bits = *src++;
			*dst = IcDoMergeRop (bits, *dst);
			dst++;
		    }
		}
		if (endmask)
		{
		    bits = *src;
		    IcDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
	    }
	}
    }
    else
    {
	if (srcX > dstX)
	{
	    leftShift = srcX - dstX;
	    rightShift = FB_UNIT - leftShift;
	}
	else
	{
	    rightShift = dstX - srcX;
	    leftShift = FB_UNIT - rightShift;
	}
	while (height--)
	{
	    src = srcLine;
	    srcLine += srcStride;
	    dst = dstLine;
	    dstLine += dstStride;
	    
	    bits1 = 0;
	    if (reverse)
	    {
		if (srcX < dstX)
		    bits1 = *--src;
		if (endmask)
		{
		    bits = IcScrRight(bits1, rightShift); 
		    if (IcScrRight(endmask, leftShift))
		    {
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
		    }
		    --dst;
		    IcDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
		    {
			bits = IcScrRight(bits1, rightShift); 
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
			--dst;
			*dst = IcDoDestInvarientMergeRop(bits);
		    }
		}
		else
		{
		    while (n--)
		    {
			bits = IcScrRight(bits1, rightShift); 
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
			--dst;
			*dst = IcDoMergeRop(bits, *dst);
		    }
		}
		if (startmask)
		{
		    bits = IcScrRight(bits1, rightShift); 
		    if (IcScrRight(startmask, leftShift))
		    {
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
		    }
		    --dst;
		    IcDoLeftMaskByteMergeRop (dst, bits, startbyte, startmask);
		}
	    }
	    else
	    {
		if (srcX > dstX)
		    bits1 = *src++;
		if (startmask)
		{
		    bits = IcScrLeft(bits1, leftShift); 
		    bits1 = *src++;
		    bits |= IcScrRight(bits1, rightShift);
		    IcDoLeftMaskByteMergeRop (dst, bits, startbyte, startmask);
		    dst++;
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
		    {
			bits = IcScrLeft(bits1, leftShift); 
			bits1 = *src++;
			bits |= IcScrRight(bits1, rightShift);
			*dst = IcDoDestInvarientMergeRop(bits);
			dst++;
		    }
		}
		else
		{
		    while (n--)
		    {
			bits = IcScrLeft(bits1, leftShift); 
			bits1 = *src++;
			bits |= IcScrRight(bits1, rightShift);
			*dst = IcDoMergeRop(bits, *dst);
			dst++;
		    }
		}
		if (endmask)
		{
		    bits = IcScrLeft(bits1, leftShift); 
		    if (IcScrLeft(endmask, rightShift))
		    {
			bits1 = *src;
			bits |= IcScrRight(bits1, rightShift);
		    }
		    IcDoRightMaskByteMergeRop (dst, bits, endbyte, endmask);
		}
	    }
	}
    }
}

#ifdef FB_24BIT

#undef DEBUG_BLT24
#ifdef DEBUG_BLT24

static unsigned long
getPixel (char *src, int x)
{
    unsigned long   l;

    l = 0;
    memcpy (&l, src + x * 3, 3);
    return l;
}
#endif

static void
IcBlt24Line (FbBits	    *src,
	     int	    srcX,

	     FbBits	    *dst,
	     int	    dstX,

	     int	    width,

	     int	    alu,
	     FbBits	    pm,
	 
	     int	    reverse)
{
#ifdef DEBUG_BLT24
    char    *origDst = (char *) dst;
    FbBits  *origLine = dst + ((dstX >> FB_SHIFT) - 1);
    int	    origNlw = ((width + FB_MASK) >> FB_SHIFT) + 3;
    int	    origX = dstX / 24;
#endif
    
    int	    leftShift, rightShift;
    FbBits  startmask, endmask;
    int	    n;
    
    FbBits  bits, bits1;
    FbBits  mask;

    int	    rot;
    IcDeclareMergeRop ();
    
    IcInitializeMergeRop (alu, FB_ALLONES);
    IcMaskBits(dstX, width, startmask, n, endmask);
#ifdef DEBUG_BLT24
    ErrorF ("dstX %d width %d reverse %d\n", dstX, width, reverse);
#endif
    if (reverse)
    {
	src += ((srcX + width - 1) >> FB_SHIFT) + 1;
	dst += ((dstX + width - 1) >> FB_SHIFT) + 1;
	rot = IcFirst24Rot (((dstX + width - 8) & FB_MASK));
	rot = IcPrev24Rot(rot);
#ifdef DEBUG_BLT24
	ErrorF ("dstX + width - 8: %d rot: %d\n", (dstX + width - 8) & FB_MASK, rot);
#endif
	srcX = (srcX + width - 1) & FB_MASK;
	dstX = (dstX + width - 1) & FB_MASK;
    }
    else
    {
	src += srcX >> FB_SHIFT;
	dst += dstX >> FB_SHIFT;
	srcX &= FB_MASK;
	dstX &= FB_MASK;
	rot = IcFirst24Rot (dstX);
#ifdef DEBUG_BLT24
	ErrorF ("dstX: %d rot: %d\n", dstX, rot);
#endif
    }
    mask = IcRot24(pm,rot);
#ifdef DEBUG_BLT24
    ErrorF ("pm 0x%x mask 0x%x\n", pm, mask);
#endif
    if (srcX == dstX)
    {
	if (reverse)
	{
	    if (endmask)
	    {
		bits = *--src;
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & endmask);
		mask = IcPrev24Pix (mask);
	    }
	    while (n--)
	    {
		bits = *--src;
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask);
		mask = IcPrev24Pix (mask);
	    }
	    if (startmask)
	    {
		bits = *--src;
		--dst;
		*dst = IcDoMaskMergeRop(bits, *dst, mask & startmask);
	    }
	}
	else
	{
	    if (startmask)
	    {
		bits = *src++;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & startmask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    while (n--)
	    {
		bits = *src++;
		*dst = IcDoMaskMergeRop (bits, *dst, mask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    if (endmask)
	    {
		bits = *src;
		*dst = IcDoMaskMergeRop(bits, *dst, mask & endmask);
	    }
	}
    }
    else
    {
	if (srcX > dstX)
	{
	    leftShift = srcX - dstX;
	    rightShift = FB_UNIT - leftShift;
	}
	else
	{
	    rightShift = dstX - srcX;
	    leftShift = FB_UNIT - rightShift;
	}
	
	bits1 = 0;
	if (reverse)
	{
	    if (srcX < dstX)
		bits1 = *--src;
	    if (endmask)
	    {
		bits = IcScrRight(bits1, rightShift); 
		if (IcScrRight(endmask, leftShift))
		{
		    bits1 = *--src;
		    bits |= IcScrLeft(bits1, leftShift);
		}
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & endmask);
		mask = IcPrev24Pix(mask);
	    }
	    while (n--)
	    {
		bits = IcScrRight(bits1, rightShift); 
		bits1 = *--src;
		bits |= IcScrLeft(bits1, leftShift);
		--dst;
		*dst = IcDoMaskMergeRop(bits, *dst, mask);
		mask = IcPrev24Pix(mask);
	    }
	    if (startmask)
	    {
		bits = IcScrRight(bits1, rightShift); 
		if (IcScrRight(startmask, leftShift))
		{
		    bits1 = *--src;
		    bits |= IcScrLeft(bits1, leftShift);
		}
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & startmask);
	    }
	}
	else
	{
	    if (srcX > dstX)
		bits1 = *src++;
	    if (startmask)
	    {
		bits = IcScrLeft(bits1, leftShift); 
		bits1 = *src++;
		bits |= IcScrRight(bits1, rightShift);
		*dst = IcDoMaskMergeRop (bits, *dst, mask & startmask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    while (n--)
	    {
		bits = IcScrLeft(bits1, leftShift); 
		bits1 = *src++;
		bits |= IcScrRight(bits1, rightShift);
		*dst = IcDoMaskMergeRop(bits, *dst, mask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    if (endmask)
	    {
		bits = IcScrLeft(bits1, leftShift); 
		if (IcScrLeft(endmask, rightShift))
		{
		    bits1 = *src;
		    bits |= IcScrRight(bits1, rightShift);
		}
		*dst = IcDoMaskMergeRop (bits, *dst, mask & endmask);
	    }
	}
    }
#ifdef DEBUG_BLT24
    {
	int firstx, lastx, x;

	firstx = origX;
	if (firstx)
	    firstx--;
	lastx = origX + width/24 + 1;
	for (x = firstx; x <= lastx; x++)
	    ErrorF ("%06x ", getPixel (origDst, x));
	ErrorF ("\n");
	while (origNlw--)
	    ErrorF ("%08x ", *origLine++);
	ErrorF ("\n");
    }
#endif
}

void
IcBlt24 (FbBits	    *srcLine,
	 IcStride   srcStride,
	 int	    srcX,

	 FbBits	    *dstLine,
	 IcStride   dstStride,
	 int	    dstX,

	 int	    width, 
	 int	    height,

	 int	    alu,
	 FbBits	    pm,

	 int	    reverse,
	 int	    upsidedown)
{
    if (upsidedown)
    {
	srcLine += (height-1) * srcStride;
	dstLine += (height-1) * dstStride;
	srcStride = -srcStride;
	dstStride = -dstStride;
    }
    while (height--)
    {
	IcBlt24Line (srcLine, srcX, dstLine, dstX, width, alu, pm, reverse);
	srcLine += srcStride;
	dstLine += dstStride;
    }
#ifdef DEBUG_BLT24
    ErrorF ("\n");
#endif
}
#endif /* FB_24BIT */

#if FB_SHIFT == FB_STIP_SHIFT + 1

/*
 * Could be generalized to FB_SHIFT > FB_STIP_SHIFT + 1 by
 * creating an ring of values stepped through for each line
 */

void
IcBltOdd (FbBits    *srcLine,
	  IcStride  srcStrideEven,
	  IcStride  srcStrideOdd,
	  int	    srcXEven,
	  int	    srcXOdd,

	  FbBits    *dstLine,
	  IcStride  dstStrideEven,
	  IcStride  dstStrideOdd,
	  int	    dstXEven,
	  int	    dstXOdd,

	  int	    width,
	  int	    height,

	  int	    alu,
	  FbBits    pm,
	  int	    bpp)
{
    FbBits  *src;
    int	    leftShiftEven, rightShiftEven;
    FbBits  startmaskEven, endmaskEven;
    int	    nmiddleEven;
    
    FbBits  *dst;
    int	    leftShiftOdd, rightShiftOdd;
    FbBits  startmaskOdd, endmaskOdd;
    int	    nmiddleOdd;

    int	    leftShift, rightShift;
    FbBits  startmask, endmask;
    int	    nmiddle;
    
    int	    srcX, dstX;
    
    FbBits  bits, bits1;
    int	    n;
    
    int    destInvarient;
    int    even;
    IcDeclareMergeRop ();

    IcInitializeMergeRop (alu, pm);
    destInvarient = IcDestInvarientMergeRop();

    srcLine += srcXEven >> FB_SHIFT;
    dstLine += dstXEven >> FB_SHIFT;
    srcXEven &= FB_MASK;
    dstXEven &= FB_MASK;
    srcXOdd &= FB_MASK;
    dstXOdd &= FB_MASK;

    IcMaskBits(dstXEven, width, startmaskEven, nmiddleEven, endmaskEven);
    IcMaskBits(dstXOdd, width, startmaskOdd, nmiddleOdd, endmaskOdd);
    
    even = 1;
    InitializeShifts(srcXEven, dstXEven, leftShiftEven, rightShiftEven);
    InitializeShifts(srcXOdd, dstXOdd, leftShiftOdd, rightShiftOdd);
    while (height--)
    {
	src = srcLine;
	dst = dstLine;
	if (even)
	{
	    srcX = srcXEven;
	    dstX = dstXEven;
	    startmask = startmaskEven;
	    endmask = endmaskEven;
	    nmiddle = nmiddleEven;
	    leftShift = leftShiftEven;
	    rightShift = rightShiftEven;
	    srcLine += srcStrideEven;
	    dstLine += dstStrideEven;
	    even = 0;
	}
	else
	{
	    srcX = srcXOdd;
	    dstX = dstXOdd;
	    startmask = startmaskOdd;
	    endmask = endmaskOdd;
	    nmiddle = nmiddleOdd;
	    leftShift = leftShiftOdd;
	    rightShift = rightShiftOdd;
	    srcLine += srcStrideOdd;
	    dstLine += dstStrideOdd;
	    even = 1;
	}
	if (srcX == dstX)
	{
	    if (startmask)
	    {
		bits = *src++;
		*dst = IcDoMaskMergeRop (bits, *dst, startmask);
		dst++;
	    }
	    n = nmiddle;
	    if (destInvarient)
	    {
		while (n--)
		{
		    bits = *src++;
		    *dst = IcDoDestInvarientMergeRop(bits);
		    dst++;
		}
	    }
	    else
	    {
		while (n--)
		{
		    bits = *src++;
		    *dst = IcDoMergeRop (bits, *dst);
		    dst++;
		}
	    }
	    if (endmask)
	    {
		bits = *src;
		*dst = IcDoMaskMergeRop(bits, *dst, endmask);
	    }
	}
	else
	{
	    bits = 0;
	    if (srcX > dstX)
		bits = *src++;
	    if (startmask)
	    {
		bits1 = IcScrLeft(bits, leftShift);
		bits = *src++;
		bits1 |= IcScrRight(bits, rightShift);
		*dst = IcDoMaskMergeRop (bits1, *dst, startmask);
		dst++;
	    }
	    n = nmiddle;
	    if (destInvarient)
	    {
		while (n--)
		{
		    bits1 = IcScrLeft(bits, leftShift);
		    bits = *src++;
		    bits1 |= IcScrRight(bits, rightShift);
		    *dst = IcDoDestInvarientMergeRop(bits1);
		    dst++;
		}
	    }
	    else
	    {
		while (n--)
		{
		    bits1 = IcScrLeft(bits, leftShift);
		    bits = *src++;
		    bits1 |= IcScrRight(bits, rightShift);
		    *dst = IcDoMergeRop(bits1, *dst);
		    dst++;
		}
	    }
	    if (endmask)
	    {
		bits1 = IcScrLeft(bits, leftShift);
		if (IcScrLeft(endmask, rightShift))
		{
		    bits = *src;
		    bits1 |= IcScrRight(bits, rightShift);
		}
		*dst = IcDoMaskMergeRop (bits1, *dst, endmask);
	    }
	}
    }
}

#ifdef FB_24BIT
void
IcBltOdd24 (FbBits	*srcLine,
	    IcStride	srcStrideEven,
	    IcStride	srcStrideOdd,
	    int		srcXEven,
	    int		srcXOdd,

	    FbBits	*dstLine,
	    IcStride	dstStrideEven,
	    IcStride	dstStrideOdd,
	    int		dstXEven,
	    int		dstXOdd,

	    int		width,
	    int		height,

	    int		alu,
	    FbBits	pm)
{
    int    even = 1;
    
    while (height--)
    {
	if (even)
	{
	    IcBlt24Line (srcLine, srcXEven, dstLine, dstXEven,
			 width, alu, pm, 0);
	    srcLine += srcStrideEven;
	    dstLine += dstStrideEven;
	    even = 0;
	}
	else
	{
	    IcBlt24Line (srcLine, srcXOdd, dstLine, dstXOdd,
			 width, alu, pm, 0);
	    srcLine += srcStrideOdd;
	    dstLine += dstStrideOdd;
	    even = 1;
	}
    }
#if 0
    fprintf (stderr, "\n");
#endif
}
#endif

#endif

#if FB_STIP_SHIFT != FB_SHIFT
void
IcSetBltOdd (IcStip	*stip,
	     IcStride	stipStride,
	     int	srcX,
	     FbBits	**bits,
	     IcStride	*strideEven,
	     IcStride	*strideOdd,
	     int	*srcXEven,
	     int	*srcXOdd)
{
    int	    srcAdjust;
    int	    strideAdjust;

    /*
     * bytes needed to align source
     */
    srcAdjust = (((int) stip) & (FB_MASK >> 3));
    /*
     * IcStip units needed to align stride
     */
    strideAdjust = stipStride & (FB_MASK >> FB_STIP_SHIFT);

    *bits = (FbBits *) ((char *) stip - srcAdjust);
    if (srcAdjust)
    {
	*strideEven = IcStipStrideToBitsStride (stipStride + 1);
	*strideOdd = IcStipStrideToBitsStride (stipStride);

	*srcXEven = srcX + (srcAdjust << 3);
	*srcXOdd = srcX + (srcAdjust << 3) - (strideAdjust << FB_STIP_SHIFT);
    }
    else
    {
	*strideEven = IcStipStrideToBitsStride (stipStride);
	*strideOdd = IcStipStrideToBitsStride (stipStride + 1);
	
	*srcXEven = srcX;
	*srcXOdd = srcX + (strideAdjust << FB_STIP_SHIFT);
    }
}
#endif

void
IcBltStip (IcStip   *src,
	   IcStride srcStride,	    /* in IcStip units, not FbBits units */
	   int	    srcX,
	   
	   IcStip   *dst,
	   IcStride dstStride,	    /* in IcStip units, not FbBits units */
	   int	    dstX,

	   int	    width, 
	   int	    height,

	   int	    alu,
	   FbBits   pm,
	   int	    bpp)
{
#if FB_STIP_SHIFT != FB_SHIFT
    if (FB_STIP_ODDSTRIDE(srcStride) || FB_STIP_ODDPTR(src) ||
	FB_STIP_ODDSTRIDE(dstStride) || FB_STIP_ODDPTR(dst))
    {
	IcStride    srcStrideEven, srcStrideOdd;
	IcStride    dstStrideEven, dstStrideOdd;
	int	    srcXEven, srcXOdd;
	int	    dstXEven, dstXOdd;
	FbBits	    *s, *d;
	int	    sx, dx;
	
	src += srcX >> FB_STIP_SHIFT;
	srcX &= FB_STIP_MASK;
	dst += dstX >> FB_STIP_SHIFT;
	dstX &= FB_STIP_MASK;
	
	IcSetBltOdd (src, srcStride, srcX,
		     &s,
		     &srcStrideEven, &srcStrideOdd,
		     &srcXEven, &srcXOdd);
		     
	IcSetBltOdd (dst, dstStride, dstX,
		     &d,
		     &dstStrideEven, &dstStrideOdd,
		     &dstXEven, &dstXOdd);
		     
#ifdef FB_24BIT
	if (bpp == 24 && !IcCheck24Pix (pm))
	{
	    IcBltOdd24  (s, srcStrideEven, srcStrideOdd,
			 srcXEven, srcXOdd,

			 d, dstStrideEven, dstStrideOdd,
			 dstXEven, dstXOdd,

			 width, height, alu, pm);
	}
	else
#endif
	{
	    IcBltOdd (s, srcStrideEven, srcStrideOdd,
		      srcXEven, srcXOdd,
    
		      d, dstStrideEven, dstStrideOdd,
		      dstXEven, dstXOdd,
    
		      width, height, alu, pm, bpp);
	}
    }
    else
#endif
    {
	IcBlt ((FbBits *) src, IcStipStrideToBitsStride (srcStride), 
	       srcX, 
	       (FbBits *) dst, IcStipStrideToBitsStride (dstStride), 
	       dstX, 
	       width, height,
	       alu, pm, bpp, 0, 0);
    }
}
