/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © 2004,2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#define CAIRO_VERSION_H 1

#include "cairo-boilerplate-private.h"
#include "cairo-boilerplate-scaled-font.h"

#include <cairo-types-private.h>
#include <cairo-scaled-font-private.h>

#if CAIRO_HAS_SCRIPT_SURFACE
#include <cairo-script.h>
#endif

/* get the "real" version info instead of dummy cairo-version.h */
#undef CAIRO_VERSION_H
#include "../cairo-version.h"

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#if HAVE_UNISTD_H && HAVE_FCNTL_H && HAVE_SIGNAL_H && HAVE_SYS_STAT_H && HAVE_SYS_SOCKET_H && HAVE_SYS_UN_H
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#define HAS_DAEMON 1
#define SOCKET_PATH "./.any2ppm"
#endif

cairo_content_t
cairo_boilerplate_content (cairo_content_t content)
{
    if (content == CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED)
	content = CAIRO_CONTENT_COLOR_ALPHA;

    return content;
}

const char *
cairo_boilerplate_content_name (cairo_content_t content)
{
    /* For the purpose of the content name, we don't distinguish the
     * flattened content value.
     */
    switch (cairo_boilerplate_content (content)) {
    case CAIRO_CONTENT_COLOR:
	return "rgb24";
    case CAIRO_CONTENT_COLOR_ALPHA:
	return "argb32";
    case CAIRO_CONTENT_ALPHA:
    default:
	assert (0); /* not reached */
	return "---";
    }
}

cairo_format_t
cairo_boilerplate_format_from_content (cairo_content_t content)
{
    cairo_format_t format;

    switch (content) {
	case CAIRO_CONTENT_COLOR: format = CAIRO_FORMAT_RGB24; break;
	case CAIRO_CONTENT_COLOR_ALPHA: format = CAIRO_FORMAT_ARGB32; break;
	case CAIRO_CONTENT_ALPHA: format = CAIRO_FORMAT_A8; break;
	default:
	    assert (0); /* not reached */
	    format = (cairo_format_t) -1;
	    break;
    }

    return format;
}

static cairo_surface_t *
_cairo_boilerplate_image_create_surface (const char			 *name,
					 cairo_content_t		  content,
					 double				  width,
					 double				  height,
					 double				  max_width,
					 double				  max_height,
					 cairo_boilerplate_mode_t	  mode,
					 int                              id,
					 void				**closure)
{
    cairo_format_t format;

    *closure = NULL;

    if (content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = CAIRO_FORMAT_ARGB32;
    } else if (content == CAIRO_CONTENT_COLOR) {
	format = CAIRO_FORMAT_RGB24;
    } else {
	assert (0); /* not reached */
	return NULL;
    }

    return cairo_image_surface_create (format, ceil (width), ceil (height));
}

static cairo_surface_t *
_cairo_boilerplate_meta_create_surface (const char	     *name,
					cairo_content_t	      content,
					double		      width,
					double		      height,
					double		      max_width,
					double		      max_height,
					cairo_boilerplate_mode_t mode,
					int		      id,
					void		    **closure)
{
    cairo_rectangle_t extents;

    *closure = NULL;

    extents.x = 0;
    extents.y = 0;
    extents.width = width;
    extents.height = height;
    return cairo_meta_surface_create (content, &extents);
}

const cairo_user_data_key_t cairo_boilerplate_output_basename_key;

#if CAIRO_HAS_SCRIPT_SURFACE
static cairo_status_t
stdio_write (void *closure, const unsigned char *data, unsigned int len)
{
    if (fwrite (data, len, 1, closure) != 1)
	return CAIRO_STATUS_WRITE_ERROR;

    return CAIRO_STATUS_SUCCESS;
}
#endif

cairo_surface_t *
_cairo_boilerplate_get_image_surface (cairo_surface_t *src,
				      int page,
				      int width,
				      int height)
{
    FILE *file = NULL;
    cairo_surface_t *surface, *image;
    cairo_t *cr;
    cairo_status_t status;

    if (cairo_surface_status (src))
	return cairo_surface_reference (src);

    if (page != 0)
	return cairo_boilerplate_surface_create_in_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);

    /* extract sub-surface */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    image = cairo_surface_reference (surface);

    /* open a logging channel (only interesting for meta surfaces) */
#if CAIRO_HAS_SCRIPT_SURFACE
    if (cairo_surface_get_type (src) == CAIRO_SURFACE_TYPE_META) {
	const char *test_name;

	test_name = cairo_surface_get_user_data (src,
						 &cairo_boilerplate_output_basename_key);
	if (test_name != NULL) {
	    char *filename;

	    xasprintf (&filename, "%s.out.trace", test_name);
	    file = fopen (filename, "w");
	    free (filename);

	    if (file != NULL) {
		surface = cairo_script_surface_create_for_target (image,
								  stdio_write,
								  file);
	    }
	}
    }
#endif

    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_set_source_surface (cr, src, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    status = cairo_status (cr);
    if (status) {
	cairo_surface_destroy (image);
	image = cairo_surface_reference (cairo_get_target (cr));
    }
    cairo_destroy (cr);

    if (file != NULL)
	fclose (file);

    return image;
}

cairo_surface_t *
cairo_boilerplate_get_image_surface_from_png (const char *filename,
					      int width,
					      int height,
					      cairo_bool_t flatten)
{
    cairo_surface_t *surface;

    surface = cairo_image_surface_create_from_png (filename);
    if (cairo_surface_status (surface))
	return surface;

    if (flatten) {
	cairo_t *cr;
	cairo_surface_t *flattened;

	flattened = cairo_image_surface_create (cairo_image_surface_get_format (surface),
						width,
						height);
	cr = cairo_create (flattened);
	cairo_surface_destroy (flattened);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	cairo_set_source_surface (cr, surface,
				  width - cairo_image_surface_get_width (surface),
				  height - cairo_image_surface_get_height (surface));
	cairo_paint (cr);

	cairo_surface_destroy (surface);
	surface = cairo_surface_reference (cairo_get_target (cr));
	cairo_destroy (cr);
    } else if (cairo_image_surface_get_width (surface) != width ||
	       cairo_image_surface_get_height (surface) != height)
    {
	cairo_t *cr;
	cairo_surface_t *sub;

	sub = cairo_image_surface_create (cairo_image_surface_get_format (surface),
					  width,
					  height);
	cr = cairo_create (sub);
	cairo_surface_destroy (sub);

	cairo_set_source_surface (cr, surface,
				  width - cairo_image_surface_get_width (surface),
				  height - cairo_image_surface_get_height (surface));
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (cr);

	cairo_surface_destroy (surface);
	surface = cairo_surface_reference (cairo_get_target (cr));
	cairo_destroy (cr);
    }

    return surface;
}

static const cairo_boilerplate_target_t builtin_targets[] = {
    /* I'm uncompromising about leaving the image backend as 0
     * for tolerance. There shouldn't ever be anything that is out of
     * our control here. */
    {
	"image", "image", NULL, NULL,
	CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR_ALPHA, 0,
	_cairo_boilerplate_image_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png
    },
    {
	"image", "image", NULL, NULL,
	CAIRO_SURFACE_TYPE_IMAGE, CAIRO_CONTENT_COLOR, 0,
	_cairo_boilerplate_image_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png
    },
    {
	"meta", "image", NULL, NULL,
	CAIRO_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR_ALPHA, 0,
	_cairo_boilerplate_meta_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	NULL, NULL,
	FALSE, TRUE
    },
    {
	"meta", "image", NULL, NULL,
	CAIRO_SURFACE_TYPE_META, CAIRO_CONTENT_COLOR, 0,
	_cairo_boilerplate_meta_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	NULL, NULL,
	FALSE, TRUE
    },
};
CAIRO_BOILERPLATE (builtin, builtin_targets)

static struct cairo_boilerplate_target_list {
    struct cairo_boilerplate_target_list *next;
    const cairo_boilerplate_target_t *target;
} *cairo_boilerplate_targets;

void
_cairo_boilerplate_register_backend (const cairo_boilerplate_target_t *targets,
				     unsigned int count)
{
    targets += count;
    while (count--) {
	struct cairo_boilerplate_target_list *list;

	list = xmalloc (sizeof (*list));
	list->next = cairo_boilerplate_targets;
	list->target = --targets;
	cairo_boilerplate_targets = list;
    }
}

const cairo_boilerplate_target_t **
cairo_boilerplate_get_targets (int *pnum_targets, cairo_bool_t *plimited_targets)
{
    size_t i, num_targets;
    cairo_bool_t limited_targets = FALSE;
    const char *tname;
    const cairo_boilerplate_target_t **targets_to_test;
    struct cairo_boilerplate_target_list *list;

    if (cairo_boilerplate_targets == NULL)
	_cairo_boilerplate_register_all ();

    if ((tname = getenv ("CAIRO_TEST_TARGET")) != NULL && *tname) {
	/* check the list of targets specified by the user */
	limited_targets = TRUE;

	num_targets = 0;
	targets_to_test = NULL;

	while (*tname) {
	    int found = 0;
	    const char *end = strpbrk (tname, " \t\r\n;:,");
	    if (!end)
	        end = tname + strlen (tname);

	    if (end == tname) {
		tname = end + 1;
		continue;
	    }

	    for (list = cairo_boilerplate_targets;
		 list != NULL;
		 list = list->next)
	    {
		const cairo_boilerplate_target_t *target = list->target;
		if (0 == strncmp (target->name, tname, end - tname) &&
		    !isalnum (target->name[end - tname])) {
		    /* realloc isn't exactly the best thing here, but meh. */
		    targets_to_test = xrealloc (targets_to_test, sizeof(cairo_boilerplate_target_t *) * (num_targets+1));
		    targets_to_test[num_targets++] = target;
		    found = 1;
		}
	    }

	    if (!found) {
		const char *last_name = NULL;

		fprintf (stderr, "Cannot find target '%.*s'.\n",
			 (int)(end - tname), tname);
		fprintf (stderr, "Known targets:");
		for (list = cairo_boilerplate_targets;
		     list != NULL;
		     list = list->next)
		{
		    const cairo_boilerplate_target_t *target = list->target;
		    if (last_name != NULL) {
			if (strcmp (target->name, last_name) == 0) {
			    /* filter out repeats that differ in content */
			    continue;
			}
			fprintf (stderr, ",");
		    }
		    fprintf (stderr, " %s", target->name);
		    last_name = target->name;
		}
		fprintf (stderr, "\n");
		exit(-1);
	    }

	    if (*end)
	      end++;
	    tname = end;
	}
    } else {
	/* check all compiled in targets */
	num_targets = 0;
	for (list = cairo_boilerplate_targets; list != NULL; list = list->next)
	    num_targets++;

	targets_to_test = xmalloc (sizeof(cairo_boilerplate_target_t*) * num_targets);
	num_targets = 0;
	for (list = cairo_boilerplate_targets;
	     list != NULL;
	     list = list->next)
	{
	    const cairo_boilerplate_target_t *target = list->target;
	    targets_to_test[num_targets++] = target;
	}
    }

    /* exclude targets as specified by the user */
    if ((tname = getenv ("CAIRO_TEST_TARGET_EXCLUDE")) != NULL && *tname) {
	limited_targets = TRUE;

	while (*tname) {
	    int j;
	    const char *end = strpbrk (tname, " \t\r\n;:,");
	    if (!end)
	        end = tname + strlen (tname);

	    if (end == tname) {
		tname = end + 1;
		continue;
	    }

	    for (i = j = 0; i < num_targets; i++) {
		if (strncmp (targets_to_test[i]->name, tname, end - tname) ||
		    isalnum (targets_to_test[i]->name[end - tname]))
		{
		    targets_to_test[j++] = targets_to_test[i];
		}
	    }
	    num_targets = j;

	    if (*end)
	      end++;
	    tname = end;
	}
    }

    if (pnum_targets)
	*pnum_targets = num_targets;

    if (plimited_targets)
	*plimited_targets = limited_targets;

    return targets_to_test;
}

const cairo_boilerplate_target_t *
cairo_boilerplate_get_image_target (cairo_content_t content)
{
    struct cairo_boilerplate_target_list *list;

    if (cairo_boilerplate_targets == NULL)
	_cairo_boilerplate_register_all ();

    for (list = cairo_boilerplate_targets; list != NULL; list = list->next) {
	const cairo_boilerplate_target_t *target = list->target;
	if (target->expected_type == CAIRO_SURFACE_TYPE_IMAGE &&
	    target->content == content)
	{
	    return target;
	}
    }

    return NULL;
}

const cairo_boilerplate_target_t *
cairo_boilerplate_get_target_by_name (const char *name,
				      cairo_content_t content)
{
    struct cairo_boilerplate_target_list *list;

    if (cairo_boilerplate_targets == NULL)
	_cairo_boilerplate_register_all ();

    /* first return an exact match */
    for (list = cairo_boilerplate_targets; list != NULL; list = list->next) {
	const cairo_boilerplate_target_t *target = list->target;
	if (strcmp (target->name, name) == 0 &&
	    target->content == content)
	{
	    return target;
	}
    }

    /* otherwise just return a match that may differ in content */
    for (list = cairo_boilerplate_targets; list != NULL; list = list->next) {
	const cairo_boilerplate_target_t *target = list->target;
	if (strcmp (target->name, name) == 0)
	    return target;
    }

    return NULL;
}

void
cairo_boilerplate_free_targets (const cairo_boilerplate_target_t **targets)
{
    free (targets);
}

cairo_surface_t *
cairo_boilerplate_surface_create_in_error (cairo_status_t status)
{
    cairo_surface_t *surface = NULL;
    int loop = 5;

    do {
	cairo_surface_t *intermediate;
	cairo_t *cr;
	cairo_path_t path;

	intermediate = cairo_image_surface_create (CAIRO_FORMAT_A8, 0, 0);
	cr = cairo_create (intermediate);
	cairo_surface_destroy (intermediate);

	path.status = status;
	cairo_append_path (cr, &path);

	cairo_surface_destroy (surface);
	surface = cairo_surface_reference (cairo_get_target (cr));
	cairo_destroy (cr);
    } while (cairo_surface_status (surface) != status && --loop);

    return surface;
}

void
cairo_boilerplate_scaled_font_set_max_glyphs_cached (cairo_scaled_font_t *scaled_font,
						     int max_glyphs)
{
    /* XXX CAIRO_DEBUG */
}

#if HAS_DAEMON
static int
any2ppm_daemon_exists (void)
{
    struct stat st;
    int fd;
    char buf[80];
    int pid;
    int ret;

    if (stat (SOCKET_PATH, &st) < 0)
	return 0;

    fd = open (SOCKET_PATH ".pid", O_RDONLY);
    if (fd < 0)
	return 0;

    pid = 0;
    ret = read (fd, buf, sizeof (buf) - 1);
    if (ret > 0) {
	buf[ret] = '\0';
	pid = atoi (buf);
    }
    close (fd);

    return pid > 0 && kill (pid, 0) != -1;
}
#endif

FILE *
cairo_boilerplate_open_any2ppm (const char *filename,
				int page,
				unsigned int flags)
{
    char command[4096];
#if HAS_DAEMON
    int sk;
    struct sockaddr_un addr;
    int len;

    if (flags & CAIRO_BOILERPLATE_OPEN_NO_DAEMON)
	goto POPEN;

    if (! any2ppm_daemon_exists ()) {
	if (system ("./any2ppm") != 0)
	    goto POPEN;
    }

    sk = socket (PF_UNIX, SOCK_STREAM, 0);
    if (sk == -1)
	goto POPEN;

    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    strcpy (addr.sun_path, SOCKET_PATH);

    if (connect (sk, (struct sockaddr *) &addr, sizeof (addr)) == -1) {
	close (sk);
	goto POPEN;
    }

    len = sprintf (command, "%s %d\n", filename, page);
    if (write (sk, command, len) != len) {
	close (sk);
	goto POPEN;
    }

    return fdopen (sk, "r");

POPEN:
#endif
    sprintf (command, "./any2ppm %s %d", filename, page);
    return popen (command, "r");
}

static cairo_bool_t
freadn (unsigned char *buf, int len, FILE *file)
{
    int ret;

    while (len) {
	ret = fread (buf, 1, len, file);
	if (ret != len) {
	    if (ferror (file) || feof (file))
		return FALSE;
	}
	len -= ret;
	buf += len;
    }

    return TRUE;
}

cairo_surface_t *
cairo_boilerplate_image_surface_create_from_ppm_stream (FILE *file)
{
    char format;
    int width, height, stride;
    int x, y;
    unsigned char *data;
    cairo_surface_t *image = NULL;

    if (fscanf (file, "P%c %d %d 255\n", &format, &width, &height) != 3)
	goto FAIL;

    switch (format) {
    case '7': /* XXX */
	image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
	break;
    case '6':
	image = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
	break;
    case '5':
	image = cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
	break;
    default:
	goto FAIL;
    }
    if (cairo_surface_status (image))
	return image;

    data = cairo_image_surface_get_data (image);
    stride = cairo_image_surface_get_stride (image);
    for (y = 0; y < height; y++) {
	unsigned char *buf = data + y*stride;
	switch (format) {
	case '7':
	    if (! freadn (buf, 4 * width, file))
		goto FAIL;
	    break;
	case '6':
	    if (! freadn (buf, 3*width, file))
		goto FAIL;
	    buf += 3*width;
	    for (x = width; x--; ) {
		buf -= 3;
		((uint32_t *) (data + y*stride))[x] =
		    (buf[0] << 16) | (buf[1] << 8) | (buf[2] << 0);
	    }
	    break;
	case '5':
	    if (! freadn (buf, width, file))
		goto FAIL;
	    break;
	}
    }

    return image;

FAIL:
    cairo_surface_destroy (image);
    return cairo_boilerplate_surface_create_in_error (CAIRO_STATUS_READ_ERROR);
}

cairo_surface_t *
cairo_boilerplate_convert_to_image (const char *filename, int page)
{
    FILE *file;
    unsigned int flags = 0;
    cairo_surface_t *image;
    int ret;

  RETRY:
    file = cairo_boilerplate_open_any2ppm (filename, page, flags);
    if (file == NULL) {
	switch (errno) {
	case ENOMEM:
	    return cairo_boilerplate_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);
	default:
	    return cairo_boilerplate_surface_create_in_error (CAIRO_STATUS_READ_ERROR);
	}
    }

    image = cairo_boilerplate_image_surface_create_from_ppm_stream (file);
    ret = pclose (file);
    /* check for fatal errors from the interpreter */
    if (ret) { /* any2pmm should never die... */
	cairo_surface_destroy (image);
	return cairo_boilerplate_surface_create_in_error (CAIRO_STATUS_INVALID_STATUS);
    }

    if (ret == 0 && cairo_surface_status (image) == CAIRO_STATUS_READ_ERROR) {
	if (flags == 0) {
	    /* Try again in a standalone process. */
	    cairo_surface_destroy (image);
	    flags = CAIRO_BOILERPLATE_OPEN_NO_DAEMON;
	    goto RETRY;
	}
    }

    return image;
}

int
cairo_boilerplate_version (void)
{
    return CAIRO_VERSION;
}

const char*
cairo_boilerplate_version_string (void)
{
    return CAIRO_VERSION_STRING;
}
