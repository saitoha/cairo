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

#ifndef IC_H_INCLUDED
#define IC_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#include <pixregion.h>

/* NOTE: Must be manually synchronized with LIBIC_VERSION in configure.in */
#define IC_MAJOR	0
#define IC_MINOR	1
#define IC_REVISION	0

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* icformat.c */

/* XXX: Change from int to enum for IcFormatName */
typedef int IcFormatName;

/* XXX: Is depth redundant here? */
typedef struct _IcFormat {
    /* XXX: Should switch from int to an IcFormatName enum */
    int		format_name;
    int		depth;
    int		red, redMask;
    int		green, greenMask;
    int		blue, blueMask;
    int		alpha, alphaMask;
} IcFormat;

void
IcFormatInit (IcFormat *format, IcFormatName name);

/* icimage.c */

typedef struct _IcImage	IcImage;

IcImage *
IcImageCreate (IcFormat	*format,
	       int	width,
	       int	height);

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

IcImage *
IcImageCreateForData (IcBits *data, IcFormat *format, int width, int height, int bpp, int stride);

void
IcImageDestroy (IcImage *image);

int
IcImageSetClipRegion (IcImage	*image,
		      PixRegion	*region);

typedef int IcFixed16_16;

typedef struct _IcPointFixed {
    IcFixed16_16  x, y;
} IcPointFixed;

typedef struct _IcLineFixed {
    IcPointFixed	p1, p2;
} IcLineFixed;

/* XXX: It's goofy that IcRectangle has integers while all the other
   datatypes have fixed-point values. (Though by design,
   IcFillRectangles is designed to fill only whole pixels) */
typedef struct _IcRectangle {
    short x, y;
    unsigned short width, height;
} IcRectangle;

typedef struct _IcTriangle {
    IcPointFixed	p1, p2, p3;
} IcTriangle;

typedef struct _IcTrapezoid {
    IcFixed16_16  top, bottom;
    IcLineFixed	left, right;
} IcTrapezoid;

typedef struct _IcVector {
    IcFixed16_16    vector[3];
} IcVector;

typedef struct _IcTransform {
    IcFixed16_16  matrix[3][3];
} IcTransform;

typedef enum {
    IcFilterFast,
    IcFilterGood,
    IcFilterBest,
    IcFilterNearest,
    IcFilterBilinear
} IcFilter;

int
IcImageSetTransform (IcImage		*image,
		     IcTransform	*transform);

void
IcImageSetRepeat (IcImage	*image,
		  int		repeat);

void
IcImageSetFilter (IcImage	*image,
		  IcFilter	filter);

int
IcImageGetWidth (IcImage	*image);

int
IcImageGetHeight (IcImage	*image);

int
IcImageGetStride (IcImage	*image);

IcFormat *
IcImageGetFormat (IcImage	*image);

IcBits *
IcImageGetData (IcImage	*image);

/* iccolor.c */

/* XXX: Do we really need a struct here? Only IcRectangles uses this. */
typedef struct {
    unsigned short   red;
    unsigned short   green;
    unsigned short   blue;
    unsigned short   alpha;
} IcColor;

void
IcColorToPixel (const IcFormat	*format,
		const IcColor	*color,
		IcBits		*pixel);

void
IcPixelToColor (const IcFormat	*format,
		IcBits		pixel,
		IcColor		*color);

/* icrect.c */

void IcFillRectangle (char		op,
		      IcImage		*dst,
		      const IcColor	*color,
		      int		x,
		      int		y,
		      unsigned int	width,
		      unsigned int	height);
void
IcFillRectangles (char			op,
		  IcImage		*dst,
		  const IcColor		*color,
		  const IcRectangle	*rects,
		  int			nRects);

/* ictrap.c */

/* XXX: Switch to enum for op */
void
IcCompositeTrapezoids (char		op,
		       IcImage		*src,
		       IcImage		*dst,
		       int		xSrc,
		       int		ySrc,
		       const IcTrapezoid *traps,
		       int		ntrap);

/* ictri.c */

void
IcCompositeTriangles (char		op,
		      IcImage		*src,
		      IcImage		*dst,
		      int		xSrc,
		      int		ySrc,
		      const IcTriangle	*tris,
		      int		ntris);

void
IcCompositeTriStrip (char		op,
		     IcImage		*src,
		     IcImage		*dst,
		     int		xSrc,
		     int		ySrc,
		     const IcPointFixed	*points,
		     int		npoints);


void
IcCompositeTriFan (char			op,
		   IcImage		*src,
		   IcImage		*dst,
		   int			xSrc,
		   int			ySrc,
		   const IcPointFixed	*points,
		   int			npoints);

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

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* IC_H_INCLUDED */
