/*
 * $XFree86: $
 *
 * Copyright � 1998 Keith Packard
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

#ifndef _IC_H_
#define _IC_H_

#include <stdint.h>

#include "pixregion.h"

/* icformat.c */

/* XXX: Perhaps we just want an enum for some standard formats? */
typedef int IcFormatName;
typedef struct _IcFormat IcFormat;

/* XXX: Not sure if this is at all the API we want for IcFormat */
IcFormat *
IcFormatCreate (IcFormatName name);

void
IcFormatDestroy (IcFormat *format);

/* icpixels.c */

typedef struct _IcPixels IcPixels;

IcPixels *
IcPixelsCreate (int width, int height, int depth);

/*
 * This single define controls the basic size of data manipulated
 * by this software; it must be log2(sizeof (IcBits) * 8)
 */

#ifndef IC_SHIFT
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
#define IC_SHIFT 6
typedef uint64_t IcBits;
#  else
#define IC_SHIFT 5
typedef uint32_t IcBits;
#  endif
#endif

IcPixels *
IcPixelsCreateForData (IcBits *data, int width, int height, int depth, int bpp, int stride);

void
IcPixelsDestroy (IcPixels *pixels);

/* icimage.c */

typedef struct _IcImage	IcImage;

/* XXX: I'd like to drop the mask/list interfaces here, (as well as two error codes) */
IcImage *
IcImageCreate (IcFormat	*format,
	       int	width,
	       int	height,
	       Mask	vmask,
	       XID	*vlist,
	       int	*error,
	       int	*error_value);

IcImage *
IcImageCreateForPixels (IcPixels	*pixels,
			IcFormat	*format,
			Mask		vmask,
			XID		*vlist,
			int		*error,
			int		*error_value);

/* ictrap.c */

/* XXX: Switch to enum for op */
void
IcTrapezoids (char	 op,
	      IcImage	 *src,
	      IcImage	 *dst,
	      IcFormat	 *format,
	      int	 xSrc,
	      int	 ySrc,
	      int	 ntrap,
	      XTrapezoid *traps);

/* ictri.c */

void
IcTriangles (char	    op,
	     IcImage	    *src,
	     IcImage	    *dst,
	     IcFormat	    *format,
	     int	    xSrc,
	     int	    ySrc,
	     int	    ntri,
	     XTriangle	    *tris);

void
IcTriStrip (char	    op,
	    IcImage	    *src,
	    IcImage	    *dst,
	    IcFormat	    *format,
	    int	    xSrc,
	    int	    ySrc,
	    int		    npoint,
	    XPointFixed	    *points);

void
IcTriFan (char		op,
	  IcImage	*src,
	  IcImage	*dst,
	  IcFormat	*format,
	  int		xSrc,
	  int		ySrc,
	  int		npoint,
	  XPointFixed	*points);

/* ic.c */

void
IcComposite (char	op,
	     IcImage	*iSrc,
	     IcImage    *iMask,
	     IcImage    *iDst,
	     int      	xSrc,
	     int      	ySrc,
	     int      	xMask,
	     int      	yMask,
	     int      	xDst,
	     int      	yDst,
	     int	width,
	     int	height);

#endif /* _IC_H_ */
