/*
 * Copyright � 2003 Red Hat Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Red Hat Inc. not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. Red Hat Inc. makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 * RED HAT INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RED HAT INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Graydon Hoare <graydon@redhat.com>
 */

#include "cairoint.h"
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

typedef struct {
    cairo_font_t base;

    FT_Library ft_library;
    int owns_ft_library;

    FT_Face face;
    int owns_face;

    FcPattern *pattern;
} cairo_ft_font_t;

#define DOUBLE_TO_26_6(d) ((FT_F26Dot6)((d) * 64.0))
#define DOUBLE_FROM_26_6(t) ((double)(t) / 64.0)
#define DOUBLE_TO_16_16(d) ((FT_Fixed)((d) * 65536.0))
#define DOUBLE_FROM_16_16(t) ((double)(t) / 65536.0)

/* implement the platform-specific interface */

cairo_font_t *
cairo_ft_font_create (FT_Library ft_library, FcPattern *pattern)
{
    cairo_ft_font_t *f = NULL;
    char *filename = NULL;
    FT_Face face = NULL;
    int owns_face = 0;
    FcPattern *resolved = NULL;
    FcResult result = FcResultMatch;
    
    FcConfigSubstitute (0, pattern, FcMatchPattern);
    FcDefaultSubstitute (pattern);
    
    resolved = FcFontMatch (0, pattern, &result);
    if (result != FcResultMatch)
    {
        if (resolved)
            FcPatternDestroy (resolved);
        return NULL;
    }
    
    /* If the pattern has an FT_Face object, use that. */
    if (FcPatternGetFTFace (resolved, FC_FT_FACE, 0, &face) != FcResultMatch
        || face == NULL)
    {

        /* otherwise it had better have a filename */
        int open_res = 0;
        owns_face = 1;
        result = FcPatternGetString (resolved, FC_FILE, 0, (FcChar8 **)(&filename));
      
        if (result == FcResultMatch)
            open_res = FT_New_Face (ft_library, filename, 0, &face);
      
        if (face == NULL)
            return NULL;
    }

    f = (cairo_ft_font_t *) cairo_ft_font_create_for_ft_face (face);
    if (f != NULL)
        f->pattern = FcPatternDuplicate (resolved);

    f->ft_library = ft_library;
    f->owns_ft_library = 0;

    f->owns_face = owns_face;

    FcPatternDestroy (resolved);
    return (cairo_font_t *) f;
}

FT_Face
cairo_ft_font_face (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font;

    if (font == NULL)
        return NULL;

    return font->face;
}

FcPattern *
cairo_ft_font_pattern (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font;

    if (font == NULL)
        return NULL;

    return font->pattern;
}

/* implement the backend interface */

static cairo_font_t *
_cairo_ft_font_create (const char           *family, 
                       cairo_font_slant_t   slant, 
                       cairo_font_weight_t  weight)
{
    cairo_ft_font_t *ft_font = NULL;
    cairo_font_t *font = NULL;
    FcPattern * pat = NULL;
    int fcslant;
    int fcweight;
    FT_Library ft_library;
    FT_Error error;

    pat = FcPatternCreate ();
    if (pat == NULL)
        return NULL;

    switch (weight)
    {
    case CAIRO_FONT_WEIGHT_BOLD:
        fcweight = FC_WEIGHT_BOLD;
        break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
        fcweight = FC_WEIGHT_MEDIUM;
        break;
    }

    switch (slant)
    {
    case CAIRO_FONT_SLANT_ITALIC:
        fcslant = FC_SLANT_ITALIC;
        break;
    case CAIRO_FONT_SLANT_OBLIQUE:
	fcslant = FC_SLANT_OBLIQUE;
        break;
    case CAIRO_FONT_SLANT_NORMAL:
    default:
        fcslant = FC_SLANT_ROMAN;
        break;
    }

    FcPatternAddString (pat, FC_FAMILY, family);
    FcPatternAddInteger (pat, FC_SLANT, fcslant);
    FcPatternAddInteger (pat, FC_WEIGHT, fcweight);

    error = FT_Init_FreeType (&ft_library);
    if (error) {
	FcPatternDestroy (pat);
	return NULL;
    }

    font = cairo_ft_font_create (ft_library, pat);
    if (font == NULL)
	return NULL;

    ft_font = (cairo_ft_font_t *) font;

    ft_font->owns_ft_library = 1;

    FT_Set_Char_Size (ft_font->face,
                      DOUBLE_TO_26_6 (1.0),
                      DOUBLE_TO_26_6 (1.0),
                      0, 0);
  
    FcPatternDestroy (pat);
    return font;  
}

static cairo_font_t *
_cairo_ft_font_copy (void *abstract_font)
{
    cairo_ft_font_t * font_new = NULL;
    cairo_ft_font_t * font = abstract_font;
  
    if (font->base.backend != &cairo_ft_font_backend)
	return NULL;

    font_new = (cairo_ft_font_t *) cairo_ft_font_create_for_ft_face (font->face);
    if (font_new == NULL)
        return NULL;

    if (font_new != NULL && font->pattern != NULL)
        font_new->pattern = FcPatternDuplicate (font->pattern);  

    return (cairo_font_t *) font_new;
}

static void 
_cairo_ft_font_destroy (void *abstract_font)
{
    cairo_ft_font_t * font = abstract_font;
  
    if (font == NULL)
        return;

    if (font->face != NULL && font->owns_face)
        FT_Done_Face (font->face);
  
    if (font->pattern != NULL)
        FcPatternDestroy (font->pattern);

    if (font->ft_library && font->owns_ft_library)
	FT_Done_FreeType (font->ft_library);

    free (font);
}

static void 
_utf8_to_ucs4 (char const *utf8, 
               FT_ULong **ucs4, 
               size_t *nchars)
{
    int len = 0, step = 0;
    size_t n = 0, alloc = 0;
    FcChar32 u = 0;

    if (ucs4 == NULL || nchars == NULL)
        return;

    len = strlen (utf8);
    alloc = len;
    *ucs4 = malloc (sizeof (FT_ULong) * alloc);
    if (*ucs4 == NULL)
        return;
  
    while (len && (step = FcUtf8ToUcs4(utf8, &u, len)) > 0)
    {
        if (n == alloc)
        {
            alloc *= 2;
            *ucs4 = realloc (*ucs4, sizeof (FT_ULong) * alloc);
            if (*ucs4 == NULL)
                return;
        }	  
        (*ucs4)[n++] = u;
        len -= step;
        utf8 += step;
    }
    *nchars = alloc;
}

static void
_install_font_matrix(cairo_matrix_t *matrix, FT_Face face)
{
    cairo_matrix_t normalized;
    double scale_x, scale_y;
    double xx, xy, yx, yy, tx, ty;
    FT_Matrix mat;
    
    /* The font matrix has x and y "scale" components which we extract and
     * use as pixel scale values. These influence the way freetype chooses
     * hints, as well as selecting different bitmaps in hand-rendered
     * fonts. We also copy the normalized matrix to freetype's
     * transformation.
     */

    _cairo_matrix_compute_scale_factors (matrix, &scale_x, &scale_y);
    
    cairo_matrix_copy (&normalized, matrix);

    cairo_matrix_scale (&normalized, 1.0 / scale_x, 1.0 / scale_y);
    cairo_matrix_get_affine (&normalized, 
                             &xx /* 00 */ , &yx /* 01 */, 
                             &xy /* 10 */, &yy /* 11 */, 
                             &tx, &ty);

    mat.xx = DOUBLE_TO_16_16(xx);
    mat.xy = -DOUBLE_TO_16_16(xy);
    mat.yx = -DOUBLE_TO_16_16(yx);
    mat.yy = DOUBLE_TO_16_16(yy);  

    FT_Set_Transform(face, &mat, NULL);
    FT_Set_Char_Size(face,
		     DOUBLE_TO_26_6(scale_x),
		     DOUBLE_TO_26_6(scale_y),
		     0, 0);
}

static int
_utf8_to_glyphs (cairo_ft_font_t	*font,
		 const unsigned char	*utf8,
		 double			x0,
		 double			y0,
		 cairo_glyph_t		**glyphs,
		 size_t			*nglyphs)
{
    FT_Face face = font->face;
    double x = 0., y = 0.;
    size_t i;
    FT_ULong *ucs4 = NULL; 

    _utf8_to_ucs4 (utf8, &ucs4, nglyphs);

    if (ucs4 == NULL)
        return 0;

    *glyphs = (cairo_glyph_t *) malloc ((*nglyphs) * (sizeof (cairo_glyph_t)));
    if (*glyphs == NULL)
    {
        free (ucs4);
        return 0;
    }

    _install_font_matrix (&font->base.matrix, face);

    for (i = 0; i < *nglyphs; i++)
    {            
        (*glyphs)[i].index = FT_Get_Char_Index (face, ucs4[i]);
        (*glyphs)[i].x = x0 + x;
        (*glyphs)[i].y = y0 + y;

        FT_Load_Glyph (face, (*glyphs)[i].index, FT_LOAD_DEFAULT);
	
        x += DOUBLE_FROM_26_6 (face->glyph->advance.x);
        y -= DOUBLE_FROM_26_6 (face->glyph->advance.y);
    }

    free (ucs4);
    return 1;
}

static cairo_status_t 
_cairo_ft_font_font_extents (void			*abstract_font,
                             cairo_font_extents_t	*extents)
{
    cairo_ft_font_t *font = abstract_font;
    FT_Face face = font->face;
    double scale_x, scale_y;

    double upm = face->units_per_EM;

    _cairo_matrix_compute_scale_factors (&font->base.matrix, &scale_x, &scale_y);

    extents->ascent =        face->ascender / upm * scale_y;
    extents->descent =       face->descender / upm * scale_y;
    extents->height =        face->height / upm * scale_y;
    extents->max_x_advance = face->max_advance_width / upm * scale_x;
    extents->max_y_advance = face->max_advance_height / upm * scale_y;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_glyph_extents (void			*abstract_font,
                              cairo_glyph_t		*glyphs, 
                              int			num_glyphs,
			      cairo_text_extents_t	*extents)
{
    int i;
    cairo_ft_font_t *font = abstract_font;
    cairo_point_double_t origin;
    cairo_point_double_t glyph_min, glyph_max;
    cairo_point_double_t total_min, total_max;
    FT_Error error;
    FT_Face face = font->face;
    FT_GlyphSlot glyph = face->glyph;
    FT_Glyph_Metrics *metrics = &glyph->metrics;

    if (num_glyphs == 0)
    {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width  = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;

	return CAIRO_STATUS_SUCCESS;
    }

    origin.x = glyphs[0].x;
    origin.y = glyphs[0].y;

    _install_font_matrix (&font->base.matrix, face);

    for (i = 0; i < num_glyphs; i++)
    {
	error = FT_Load_Glyph (face, glyphs[i].index, FT_LOAD_DEFAULT);
	/* XXX: What to do in this error case? */
	if (error)
	    continue;

	/* XXX: Need to add code here to check the font's FcPattern
           for FC_VERTICAL_LAYOUT and if set get vertBearingX/Y
           instead. This will require that
           cairo_ft_font_create_for_ft_face accept an
           FcPattern. */
	glyph_min.x = glyphs[i].x + DOUBLE_FROM_26_6 (metrics->horiBearingX);
	glyph_min.y = glyphs[i].y - DOUBLE_FROM_26_6 (metrics->horiBearingY);
	glyph_max.x = glyph_min.x + DOUBLE_FROM_26_6 (metrics->width);
	glyph_max.y = glyph_min.y + DOUBLE_FROM_26_6 (metrics->height);
    
	if (i==0) {
	    total_min = glyph_min;
	    total_max = glyph_max;
	} else {
	    if (glyph_min.x < total_min.x)
		total_min.x = glyph_min.x;
	    if (glyph_min.y < total_min.y)
		total_min.y = glyph_min.y;

	    if (glyph_max.x > total_max.x)
		total_max.x = glyph_max.x;
	    if (glyph_max.y > total_max.y)
		total_max.y = glyph_max.y;
	}
    }

    extents->x_bearing = total_min.x - origin.x;
    extents->y_bearing = total_min.y - origin.y;
    extents->width     = total_max.x - total_min.x;
    extents->height    = total_max.y - total_min.y;
    extents->x_advance = glyphs[i-1].x + DOUBLE_FROM_26_6 (metrics->horiAdvance) - origin.x;
    extents->y_advance = glyphs[i-1].y + 0 - origin.y;

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_text_extents (void 			*abstract_font,
                             const unsigned char	*utf8,
			     cairo_text_extents_t	*extents)
{
    cairo_ft_font_t *font = abstract_font;
    cairo_glyph_t *glyphs;
    size_t nglyphs;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (_utf8_to_glyphs (font, utf8, 0, 0, &glyphs, &nglyphs))
    {
        status = _cairo_ft_font_glyph_extents (font, glyphs, nglyphs, 
					       extents);      
        free (glyphs);
    }
    return status;
}

static cairo_status_t 
_cairo_ft_font_show_glyphs (void		*abstract_font,
                            cairo_operator_t    operator,
                            cairo_surface_t     *source,
                            cairo_surface_t     *surface,
                            const cairo_glyph_t *glyphs,
                            int                 num_glyphs)
{
    cairo_ft_font_t *font = abstract_font;
    cairo_status_t status;
    int i;
    cairo_ft_font_t *ft = NULL;
    FT_GlyphSlot glyphslot;
    cairo_surface_t *mask = NULL;

    double x, y;
    int width, height, stride;

    if (font == NULL 
        || source == NULL 
        || surface == NULL 
        || glyphs == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    ft = (cairo_ft_font_t *)font;
    glyphslot = ft->face->glyph;
    _install_font_matrix (&font->base.matrix, ft->face);

    for (i = 0; i < num_glyphs; i++)
    {
	unsigned char *bitmap;

        FT_Load_Glyph (ft->face, glyphs[i].index, FT_LOAD_DEFAULT);
        FT_Render_Glyph (glyphslot, FT_RENDER_MODE_NORMAL);
      
        width = glyphslot->bitmap.width;
        height = glyphslot->bitmap.rows;
        stride = glyphslot->bitmap.pitch;
	bitmap = glyphslot->bitmap.buffer;
   
	x = glyphs[i].x;
	y = glyphs[i].y;      

        /* X gets upset with zero-sized images (such as whitespace) */
        if (width * height == 0)
	    continue;
	    
	/*
	 * XXX 
	 * reformat to match libic alignment requirements.
	 * This should be done before rendering the glyph,
	 * but that requires using FT_Outline_Get_Bitmap
	 * function
	 */
	if (stride & 3)
	{
	    int		nstride = (stride + 3) & ~3;
	    unsigned char	*g, *b;
	    int		h;
	    
	    bitmap = malloc (nstride * height);
	    if (!bitmap)
		return CAIRO_STATUS_NO_MEMORY;
	    g = glyphslot->bitmap.buffer;
	    b = bitmap;
	    h = height;
	    while (h--)
	    {
		memcpy (b, g, width);
		b += nstride;
		g += stride;
	    }
	    stride = nstride;
	}
	mask = cairo_surface_create_for_image (bitmap,
					       CAIRO_FORMAT_A8,
					       width, height, stride);
	if (mask == NULL)
	{
	    if (bitmap != glyphslot->bitmap.buffer)
		free (bitmap);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	
	status = _cairo_surface_composite (operator, source, mask, surface,
					   0, 0, 0, 0, 
					   x + glyphslot->bitmap_left, 
					   y - glyphslot->bitmap_top, 
					   (double)width, (double)height);
	
	cairo_surface_destroy (mask);
	if (bitmap != glyphslot->bitmap.buffer)
	    free (bitmap);
	
	if (status)
	    return status;
    }  
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_show_text (void		      *abstract_font,
                          cairo_operator_t    operator,
                          cairo_surface_t     *source,
                          cairo_surface_t     *surface,
                          double              x0,
                          double              y0,
                          const unsigned char *utf8)
{
    cairo_ft_font_t *font = abstract_font;
    cairo_glyph_t *glyphs;
    int num_glyphs;
    
    if (_utf8_to_glyphs (font, utf8, x0, y0, &glyphs, &num_glyphs))
    {
        cairo_status_t res;
        res = _cairo_ft_font_show_glyphs (font, operator, 
                                          source, surface,
                                          glyphs, num_glyphs);      
        free (glyphs);
        return res;
    }
    else
        return CAIRO_STATUS_NO_MEMORY;
}

static int
_move_to (FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;

    _cairo_path_close_path (path);
    _cairo_path_move_to (path,
			 DOUBLE_FROM_26_6(to->x),
			 DOUBLE_FROM_26_6(to->y));

    return 0;
}

static int
_line_to (FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;

    _cairo_path_line_to (path,
			 DOUBLE_FROM_26_6(to->x),
			 DOUBLE_FROM_26_6(to->y));

    return 0;
}

static int
_conic_to (FT_Vector *control, FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;

    double x1, y1;
    double x2 = DOUBLE_FROM_26_6(control->x);
    double y2 = DOUBLE_FROM_26_6(control->y);
    double x3 = DOUBLE_FROM_26_6(to->x);
    double y3 = DOUBLE_FROM_26_6(to->y);

    _cairo_path_current_point (path, &x1, &y1);

    _cairo_path_curve_to (path,
			  x1 + 2.0/3.0 * (x2 - x1), y1 + 2.0/3.0 * (y2 - y1),
			  x3 + 2.0/3.0 * (x2 - x3), y3 + 2.0/3.0 * (y2 - y3),
			  x3, y3);

    return 0;
}

static int
_cubic_to (FT_Vector *control1, FT_Vector *control2, FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;

    _cairo_path_curve_to (path, 
			  DOUBLE_FROM_26_6(control1->x), DOUBLE_FROM_26_6(control1->y),
			  DOUBLE_FROM_26_6(control2->x), DOUBLE_FROM_26_6(control2->y),
			  DOUBLE_FROM_26_6(to->x), DOUBLE_FROM_26_6(to->y));

    return 0;
}

static cairo_status_t 
_cairo_ft_font_glyph_path (void			*abstract_font,
                           cairo_glyph_t	*glyphs, 
                           int			num_glyphs,
                           cairo_path_t		*path)
{
    int i;
    cairo_ft_font_t *font = abstract_font;
    FT_GlyphSlot glyph;
    FT_Error error;
    FT_Outline_Funcs outline_funcs = {
	_move_to,
	_line_to,
	_conic_to,
	_cubic_to,
	0, /* shift */
	0, /* delta */
    };

    glyph = font->face->glyph;
    _install_font_matrix (&font->base.matrix, font->face);

    for (i = 0; i < num_glyphs; i++)
    {
	FT_Matrix invert_y = {
	    DOUBLE_TO_16_16 (1.0), 0,
	    0, DOUBLE_TO_16_16 (-1.0),
	};

	error = FT_Load_Glyph (font->face, glyphs[i].index, FT_LOAD_DEFAULT);
	/* XXX: What to do in this error case? */
	if (error)
	    continue;
	/* XXX: Do we want to support bitmap fonts here? */
	if (glyph->format == ft_glyph_format_bitmap)
	    continue;

	/* Font glyphs have an inverted Y axis compared to cairo. */
	FT_Outline_Transform (&glyph->outline, &invert_y);
	FT_Outline_Translate (&glyph->outline,
			      DOUBLE_TO_26_6(glyphs[i].x),
			      DOUBLE_TO_26_6(glyphs[i].y));
	FT_Outline_Decompose (&glyph->outline, &outline_funcs, path);
    }
    _cairo_path_close_path (path);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_text_path (void		      *abstract_font,
			  double	      x,
			  double	      y,
                          const unsigned char *utf8,
                          cairo_path_t        *path)
{
    cairo_ft_font_t *font = abstract_font;
    cairo_glyph_t *glyphs;
    size_t nglyphs;
    
    if (_utf8_to_glyphs (font, utf8, x, y, &glyphs, &nglyphs))
    {
        cairo_status_t res;
        res = _cairo_ft_font_glyph_path (font, glyphs, nglyphs, path);
        free (glyphs);
        return res;
    }
    else
        return CAIRO_STATUS_NO_MEMORY;
}

cairo_font_t *
cairo_ft_font_create_for_ft_face (FT_Face face)
{
    cairo_ft_font_t *f = NULL;
    
    f = malloc (sizeof (cairo_ft_font_t));
    if (f == NULL)
	return NULL;
    memset (f, 0, sizeof (cairo_ft_font_t));
    
    _cairo_font_init (&f->base, 
		      &cairo_ft_font_backend);
    
    f->ft_library = NULL;
    f->owns_ft_library = 0;

    f->face = face;
    f->owns_face = 0;

    return (cairo_font_t *) f;
}

const struct cairo_font_backend cairo_ft_font_backend = {
    _cairo_ft_font_create,
    _cairo_ft_font_copy,
    _cairo_ft_font_destroy,
    _cairo_ft_font_font_extents,
    _cairo_ft_font_text_extents,
    _cairo_ft_font_glyph_extents,
    _cairo_ft_font_show_text,
    _cairo_ft_font_show_glyphs,
    _cairo_ft_font_text_path,
    _cairo_ft_font_glyph_path,
};
