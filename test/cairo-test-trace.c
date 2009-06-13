/*
 * Copyright © 2009 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * the authors not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The authors make no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Chris Wilson <chris@chris-wilson.co.uk>
 */

/*
 * The basic idea is that we feed the trace to multiple backends in parallel
 * and compare the output at the end of each context (based on the premise
 * that contexts demarcate expose events, or their logical equivalents) with
 * that of the image[1] backend. Each backend is executed in a separate
 * process, for robustness and to isolate the global cairo state, with the
 * image data residing in shared memory and synchronising over a socket.
 *
 * [1] Should be reference implementation, currently the image backend is
 *     considered to be the reference for all other backends.
 */

/* XXX Can't directly compare fills using spans versus trapezoidation,
 *     i.e. xlib vs image. Gah, kinda renders this whole scheme moot.
 *     How about reference platforms?
 *     E.g. accelerated xlib driver vs Xvfb?
 *
 *     boilerplate->create_reference_surface()?
 *     boilerplate->reference->create_surface()?
 *     So for each backend spawn two processes, a reference and xlib
 *     (obviously minimising the number of reference processes when possible)
 */

/*
 * XXX Handle show-page as well as cairo_destroy()? Though arguably that is
 *     only relevant for paginated backends which is currently outside the
 *     scope of this test.
 */

#define _GNU_SOURCE 1	/* getline() */

#include "cairo-test.h"

#include "cairo-boilerplate-getopt.h"
#include <cairo-script-interpreter.h>

/* For basename */
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#include <ctype.h> /* isspace() */

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <errno.h>
#include <assert.h>

#if HAVE_FCFINI
#include <fontconfig/fontconfig.h>
#endif

#define DATA_SIZE (64 << 20)
#define SHM_PATH_XXX "/shmem-cairo-trace"

typedef struct _test_runner {
    /* Options from command-line */
    cairo_bool_t list_only;
    char **names;
    unsigned int num_names;
    char **exclude_names;
    unsigned int num_exclude_names;

    /* Stuff used internally */
    const cairo_boilerplate_target_t **targets;
    int num_targets;
} test_runner_t;

typedef struct _test_runner_thread {
    const cairo_boilerplate_target_t *target;
    cairo_surface_t *surface;
    void *closure;
    uint8_t *base;
    pid_t pid;
    int sk;

    struct context_list {
	struct context_list *next;
	unsigned long id;
	cairo_t *context;
	cairo_surface_t *surface;
    } *contexts;

    unsigned long context_id;
} test_runner_thread_t;

struct slave {
    pid_t pid;
    int fd;
    unsigned long image_serial;
    unsigned long image_ready;
    cairo_surface_t *image;
    const cairo_boilerplate_target_t *target;
};

struct request_image {
    unsigned long id;
    cairo_format_t format;
    int width;
    int height;
    int stride;
};

struct surface_tag {
    int width, height;
};
static const cairo_user_data_key_t surface_tag;

static cairo_bool_t
writen (int fd, const void *ptr, int len)
{
#if 0
    const uint8_t *data = ptr;
    while (len) {
	int ret = write (fd, data, len);
	if (ret < 0) {
	    switch (errno) {
	    case EAGAIN:
	    case EINTR:
		continue;
	    default:
		return FALSE;
	    }
	} else if (ret == 0) {
	    return FALSE;
	} else {
	    data += ret;
	    len -= ret;
	}
    }
    return TRUE;
#else
    int ret = send (fd, ptr, len, 0);
    return ret == len;
#endif
}

static cairo_bool_t
readn (int fd, void *ptr, int len)
{
#if 0
    uint8_t *data = ptr;
    while (len) {
	int ret = read (fd, data, len);
	if (ret < 0) {
	    switch (errno) {
	    case EAGAIN:
	    case EINTR:
		continue;
	    default:
		return FALSE;
	    }
	} else if (ret == 0) {
	    return FALSE;
	} else {
	    data += ret;
	    len -= ret;
	}
    }
    return TRUE;
#else
    int ret = recv (fd, ptr, len, MSG_WAITALL);
    return ret == len;
#endif
}

static cairo_format_t
format_for_content (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_ALPHA:
	return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR:
	return CAIRO_FORMAT_RGB24;
    default:
    case CAIRO_CONTENT_COLOR_ALPHA:
	return CAIRO_FORMAT_ARGB32;
    }
}

static void *
request_image (test_runner_thread_t *thread,
	       unsigned long id,
	       cairo_format_t format,
	       int width, int height, int stride)
{
    struct request_image rq = { id, format, width, height, stride };
    size_t offset = -1;

    writen (thread->sk, &rq, sizeof (rq));
    readn (thread->sk, &offset, sizeof (offset));
    if (offset == (size_t) -1)
	return NULL;

    return thread->base + offset;
}

static void
push_surface (test_runner_thread_t *thread,
	      cairo_surface_t *source,
	      unsigned long id)
{
    cairo_surface_t *image;
    cairo_format_t format = (cairo_format_t) -1;
    cairo_t *cr;
    int width, height, stride;
    void *data;
    unsigned long serial;

    if (cairo_surface_get_type (source) == CAIRO_SURFACE_TYPE_IMAGE) {
	width = cairo_image_surface_get_width (source);
	height = cairo_image_surface_get_height (source);
	format = cairo_image_surface_get_format (source);
    } else {
	struct surface_tag *tag;

	tag = cairo_surface_get_user_data (source, &surface_tag);
	if (tag != NULL) {
	    width = tag->width;
	    height = tag->height;
	} else {
	    double x0, x1, y0, y1;

	    /* presumably created using cairo_surface_create_similar() */
	    cr = cairo_create (source);
	    cairo_clip_extents (cr, &x0, &y0, &x1, &y1);
	    cairo_destroy (cr);

	    tag = xmalloc (sizeof (*tag));
	    width = tag->width = x1 - x0;
	    height = tag->height = y1 - y0;

	    if (cairo_surface_set_user_data (source, &surface_tag, tag, free))
		exit (-1);
	}
    }
    if (format == (cairo_format_t) -1)
	format = format_for_content (cairo_surface_get_content (source));

    stride = cairo_format_stride_for_width (format, width);

    data = request_image (thread, id, format, width, height, stride);
    if (data == NULL)
	exit (-1);

    image = cairo_image_surface_create_for_data (data,
						 format,
						 width, height,
						 stride);
    cr = cairo_create (image);
    cairo_surface_destroy (image);

    cairo_set_source_surface (cr, source, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    /* signal completion */
    writen (thread->sk, &id, sizeof (id));

    /* wait for image check */
    serial = 0;
    readn (thread->sk, &serial, sizeof (serial));
    if (serial != id)
	exit (-1);
}

static cairo_surface_t *
_surface_create (void *closure,
		 cairo_content_t content,
		 double width, double height)
{
    test_runner_thread_t *thread = closure;
    cairo_surface_t *surface;

    surface = cairo_surface_create_similar (thread->surface,
					    content, width, height);
    if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE) {
	struct surface_tag *tag;

	tag = xmalloc (sizeof (*tag));
	tag->width = width;
	tag->height = height;
	if (cairo_surface_set_user_data (surface, &surface_tag, tag, free))
	    exit (-1);
    }

    return surface;
}

static cairo_t *
_context_create (void *closure, cairo_surface_t *surface)
{
    test_runner_thread_t *thread = closure;
    struct context_list *l;

    l = xmalloc (sizeof (*l));
    l->next = thread->contexts;
    l->context = cairo_create (surface);
    l->surface = cairo_surface_reference (surface);
    l->id = ++thread->context_id;
    if (l->id == 0)
	l->id = ++thread->context_id;
    thread->contexts = l;

    return l->context;
}

static void
_context_destroy (void *closure, void *ptr)
{
    test_runner_thread_t *thread = closure;
    struct context_list *l, **prev = &thread->contexts;

    while ((l = *prev) != NULL) {
	if (l->context == ptr) {
	    if (cairo_surface_status (l->surface) == CAIRO_STATUS_SUCCESS) {
		push_surface (thread, l->surface, l->id);
            } else {
		fprintf (stderr, "%s: error during replay: %s!\n",
			 thread->target->name,
			 cairo_status_to_string (cairo_surface_status
						 (l->surface)));
		exit (1);
	    }

            cairo_surface_destroy (l->surface);
            *prev = l->next;
            free (l);
            return;
        }
        prev = &l->next;
    }
}

static void
execute (test_runner_thread_t    *thread,
	 const char		 *trace)
{
    const cairo_script_interpreter_hooks_t hooks = {
	.closure = thread,
	.surface_create = _surface_create,
	.context_create = _context_create,
	.context_destroy = _context_destroy,
    };
    cairo_script_interpreter_t *csi;

    csi = cairo_script_interpreter_create ();
    cairo_script_interpreter_install_hooks (csi, &hooks);

    cairo_script_interpreter_run (csi, trace);

    cairo_script_interpreter_finish (csi);
    if (cairo_script_interpreter_destroy (csi))
	exit (1);
}

static int
spawn_socket (const char *socket_path, pid_t pid)
{
    struct sockaddr_un addr;
    int sk;

    sk = socket (PF_UNIX, SOCK_STREAM, 0);
    if (sk == -1)
	return -1;

    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    strcpy (addr.sun_path, socket_path);

    if (connect (sk, (struct sockaddr *) &addr, sizeof (addr)) == -1)
	return -1;

    if (! writen (sk, &pid, sizeof (pid)))
	return -1;

    return sk;
}

static void *
spawn_shm (const char *shm_path)
{
    void *base;
    int fd;

    fd = shm_open (shm_path, O_RDWR, 0);
    if (fd == -1)
	return MAP_FAILED;

    base = mmap (NULL, DATA_SIZE,
		 PROT_READ | PROT_WRITE,
		 MAP_SHARED | MAP_NORESERVE,
		 fd, 0);
    close (fd);

    return base;
}

static int
spawn_target (const char *socket_path,
	      const char *shm_path,
	      const cairo_boilerplate_target_t *target,
	      const char *trace)
{
    test_runner_thread_t thread;
    pid_t pid;

    pid = fork ();
    if (pid != 0)
	return pid;

    thread.pid = getpid ();

    thread.sk = spawn_socket (socket_path, thread.pid);
    if (thread.sk == -1) {
	fprintf (stderr, "%s: Failed to open socket.\n",
		 target->name);
	exit (-1);
    }

    thread.base = spawn_shm (shm_path);
    if (thread.base == MAP_FAILED) {
	fprintf (stderr, "%s: Failed to map shared memory segment.\n",
		 target->name);
	exit (-1);
    }

    thread.target = target;
    thread.contexts = NULL;
    thread.context_id = 0;

    thread.surface = target->create_surface (NULL,
					     CAIRO_CONTENT_COLOR_ALPHA,
					     1, 1,
					     1, 1,
					     CAIRO_BOILERPLATE_MODE_TEST,
					     0,
					     &thread.closure);
    if (thread.surface == NULL) {
	fprintf (stderr,
		 "%s:  Failed to create target surface.\n",
		 target->name);
	exit (-1);
    }

    execute (&thread, trace);

    cairo_surface_destroy (thread.surface);

    if (target->cleanup)
	target->cleanup (thread.closure);

    close (thread.sk);
    munmap (thread.base, DATA_SIZE);

    exit (0);
}

/* XXX imagediff - is the extra expense worth it? */
static cairo_bool_t
matching_images (cairo_surface_t *a, cairo_surface_t *b)
{
    if (a == NULL || b == NULL)
	return FALSE;

    if (cairo_surface_status (a) || cairo_surface_status (b))
	return FALSE;

    if (cairo_surface_get_type (a) != cairo_surface_get_type (b))
	return FALSE;

    if (cairo_image_surface_get_format (a) != cairo_image_surface_get_format (b))
	return FALSE;

    if (cairo_image_surface_get_width (a) != cairo_image_surface_get_width (b))
	return FALSE;

    if (cairo_image_surface_get_height (a) != cairo_image_surface_get_height (b))
	return FALSE;

    if (cairo_image_surface_get_stride (a) != cairo_image_surface_get_stride (b))
	return FALSE;

    return memcmp (cairo_image_surface_get_data (a),
		   cairo_image_surface_get_data (b),
		   cairo_image_surface_get_stride (a) *
		   cairo_image_surface_get_stride (b));
}

static cairo_bool_t
check_images (struct slave *slaves, int num_slaves)
{
    int n;

    for (n = 1; n < num_slaves; n++) {
	assert (slaves[n].image_ready == slaves[0].image_ready);

	if (! matching_images (slaves[n].image, slaves[0].image))
	    return FALSE;
    }

    return TRUE;
}

static void
write_images (const char *trace, struct slave *slave, int num_slaves)
{
    while (num_slaves--) {
	if (slave->image != NULL) {
	    char *filename;

	    xasprintf (&filename, "%s-%s-out.png",
		       trace, slave->target->name);
	    cairo_surface_write_to_png (slave->image, filename);
	    free (filename);
	}

	slave++;
    }
}

static size_t
allocate_image_for_slave (uint8_t *base, size_t *offset, struct slave *slave)
{
    struct request_image rq;
    int size;
    uint8_t *data;

    readn (slave->fd, &rq, sizeof (rq));
    slave->image_serial = rq.id;

    size = rq.height * rq.stride;
    size = (size + 127) & -128;
    data = base + *offset;
    *offset += size;
    assert (*offset <= DATA_SIZE);

    assert (slave->image == NULL);
    slave->image = cairo_image_surface_create_for_data (data, rq.format,
							rq.width, rq.height,
							rq.stride);

    return data - base;
}

static cairo_bool_t
test_run (void *base,
	  int sk,
	  const char *trace,
	  struct slave *slaves,
	  int num_slaves)
{
    struct pollfd *pfd;
    int npfd, cnt, n, i;
    int completion;
    cairo_bool_t ret = FALSE;
    size_t image;

    pfd = xcalloc (num_slaves+2, sizeof (*pfd));

    pfd[0].fd = sk;
    pfd[0].events = POLLIN;
    npfd = 1;

    completion = 0;
    image = 0;
    while ((cnt = poll (pfd, npfd, -1)) > 0) {
	if (pfd[0].revents) {
	    int fd;

	    while ((fd = accept (sk, NULL, NULL)) != -1) {
		pid_t pid;

		readn (fd, &pid, sizeof (pid));
		for (n = 0; n < num_slaves; n++) {
		    if (slaves[n].pid == pid)
			slaves[n].fd = fd;
		}

		pfd[npfd].fd = fd;
		pfd[npfd].events = POLLIN;
		npfd++;
	    }
	    cnt--;
	}

	for (n = 1; n < npfd && cnt; n++) {
	    if (! pfd[n].revents)
		continue;

	    if (pfd[n].revents & POLLHUP)
		goto done;

	    for (i = 0; i < num_slaves; i++) {
		if (slaves[i].fd == pfd[n].fd) {
		    /* Communication with the slave is done in three phases,
		     * and we do each pass synchronously.
		     *
		     * 1. The slave requests an image buffer, which we
		     * allocate and then return to the slave the offset into
		     * the shared memory segment.
		     *
		     * 2. The slave indicates that it has finished writing
		     * into the shared image buffer. The slave now waits
		     * for the server to collate all the image data - thereby
		     * throttling the slaves.
		     *
		     * 3. After all slaves have finished writing their images,
		     * we compare them all against the reference image and,
		     * if satisfied, send an acknowledgement to all slaves.
		     */
		    if (slaves[i].image_serial == 0) {
			size_t offset;

			offset =
			    allocate_image_for_slave (base, &image, &slaves[i]);
			if (! writen (pfd[n].fd, &offset, sizeof (offset)))
			    goto out;
		    } else {
			readn (pfd[n].fd,
			       &slaves[i].image_ready,
			       sizeof (slaves[i].image_ready));
			if (slaves[i].image_ready != slaves[i].image_serial)
			    goto out;

			/* Can anyone spell 'P·E·D·A·N·T'? */
			cairo_surface_mark_dirty (slaves[i].image);
			completion++;
		    }

		    break;
		}
	    }

	    cnt--;
	}

	if (completion == num_slaves) {
	    if (! check_images (slaves, num_slaves)) {
		write_images (trace, slaves, num_slaves);
		goto out;
	    }

	    /* ack */
	    for (i = 0; i < num_slaves; i++) {
		cairo_surface_destroy (slaves[i].image);
		slaves[i].image = NULL;

		if (! writen (slaves[i].fd,
			      &slaves[i].image_serial,
			      sizeof (slaves[i].image_serial)))
		{
		    goto out;
		}

		slaves[i].image_serial = 0;
		slaves[i].image_ready = 0;
	    }

	    completion = 0;
	    image = 0;
	}
    }
done:
    ret = TRUE;

out:
    for (n = 0; n < num_slaves; n++) {
	if (slaves[n].fd != -1)
	    close (slaves[n].fd);

	if (slaves[n].image == NULL)
	    continue;

	cairo_surface_destroy (slaves[n].image);
	slaves[n].image = NULL;

	slaves[n].image_serial = 0;
	slaves[n].image_ready = 0;
	ret = FALSE;
    }

    free (pfd);

    return ret;
}

/* Paginated surfaces require finalization and external converters and so
 * are not suitable for this basic technique.
 */
static cairo_bool_t
target_is_measurable (const cairo_boilerplate_target_t *target)
{
    if (target->content != CAIRO_CONTENT_COLOR_ALPHA)
	return FALSE;

    switch (target->expected_type) {
    case CAIRO_SURFACE_TYPE_IMAGE:
	if (strcmp (target->name, "pdf") == 0 ||
	    strcmp (target->name, "ps") == 0)
	{
	    return FALSE;
	}
	else
	{
	    return TRUE;
	}
    case CAIRO_SURFACE_TYPE_XLIB:
    case CAIRO_SURFACE_TYPE_XCB:
    case CAIRO_SURFACE_TYPE_GLITZ:
    case CAIRO_SURFACE_TYPE_QUARTZ:
    case CAIRO_SURFACE_TYPE_WIN32:
    case CAIRO_SURFACE_TYPE_BEOS:
    case CAIRO_SURFACE_TYPE_DIRECTFB:
#if CAIRO_VERSION > CAIRO_VERSION_ENCODE(1,1,2)
    case CAIRO_SURFACE_TYPE_OS2:
#endif
	return TRUE;

    case CAIRO_SURFACE_TYPE_PDF:
    case CAIRO_SURFACE_TYPE_PS:
    case CAIRO_SURFACE_TYPE_SVG:
    case CAIRO_SURFACE_TYPE_WIN32_PRINTING:
    default:
	return FALSE;
    }

    return TRUE;
}

static int
server_socket (const char *socket_path)
{
    long flags;
    struct sockaddr_un addr;
    int sk;

    sk = socket (PF_UNIX, SOCK_STREAM, 0);
    if (sk == -1)
	return -1;

    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    strcpy (addr.sun_path, socket_path);
    if (bind (sk, (struct sockaddr *) &addr, sizeof (addr)) == -1) {
	close (sk);
	return -1;
    }

    flags = fcntl (sk, F_GETFL);
    if (flags == -1 || fcntl (sk, F_SETFL, flags | O_NONBLOCK) == -1) {
	close (sk);
	return -1;
    }

    if (listen (sk, 5) == -1) {
	close (sk);
	return -1;
    }

    return sk;
}

static int
server_shm (const char *shm_path)
{
    int fd;

    fd = shm_open (shm_path, O_RDWR | O_EXCL | O_CREAT, 0777);
    if (fd == -1)
	return -1;

    if (ftruncate (fd, DATA_SIZE) == -1) {
	close (fd);
	return -1;
    }

    return fd;
}

static cairo_bool_t
_test_trace (test_runner_t *test, const char *trace, const char *name)
{
    const char *shm_path = SHM_PATH_XXX;
    const cairo_boilerplate_target_t *target, *image;
    struct slave *slaves, *s;
    char socket_dir[] = "/tmp/cairo-test-trace.XXXXXX";
    char *socket_path;
    int sk, fd;
    int i, num_slaves;
    void *base;
    cairo_bool_t ret = FALSE;

    /* create a socket to control the test runners */
    if (mkdtemp (socket_dir) == NULL) {
	fprintf (stderr, "Unable to create temporary name for socket\n");
	return FALSE;
    }

    xasprintf (&socket_path, "%s/socket", socket_dir);
    sk = server_socket (socket_path);
    if (sk == -1) {
	fprintf (stderr, "Unable to create socket for server\n");
	goto cleanup_paths;
    }

    /* allocate some shared memory */
    fd = server_shm (shm_path);
    if (fd == -1) {
	fprintf (stderr, "Unable to create shared memory '%s'\n",
		 shm_path);
	goto cleanup_sk;
    }

    image = cairo_boilerplate_get_image_target (CAIRO_CONTENT_COLOR_ALPHA);
    assert (image != NULL);

    /* spawn slave processes to run the trace */
    s = slaves = xcalloc (test->num_targets + 1, sizeof (struct slave));
    s->pid = spawn_target (socket_path, shm_path, image, trace);
    if (s->pid < 0)
	goto cleanup;
    s->target = image;
    s->fd = -1;
    s++;

    for (i = 0; i < test->num_targets; i++) {
	pid_t slave;

	target = test->targets[i];
	if (target == image || ! target_is_measurable (target))
	    continue;

	slave = spawn_target (socket_path, shm_path, target, trace);
	if (slave < 0)
	    continue;

	s->pid = slave;
	s->target = target;
	s->fd = -1;
	s++;
    }
    num_slaves = s - slaves;
    if (num_slaves == 1) {
	fprintf (stderr, "No targets to test\n");
	goto cleanup;
    }

    base = mmap (NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
	fprintf (stderr, "Unable to mmap shared memory\n");
	goto cleanup;
    }
    ret = test_run (base, sk, name, slaves, num_slaves);
    munmap (base, DATA_SIZE);

cleanup:
    close (fd);
    while (s-- > slaves) {
	int status;
	kill (s->pid, SIGKILL);
	waitpid (s->pid, &status, 0);
	ret &= WIFEXITED (status) && WEXITSTATUS (status) == 0;
	if (WIFSIGNALED (status) && WTERMSIG(status) != SIGKILL) {
	    fprintf (stderr, "%s crashed\n", s->target->name);
	}
    }
    free (slaves);
    shm_unlink (shm_path);
cleanup_sk:
    close (sk);

cleanup_paths:
    remove (socket_path);
    remove (socket_dir);

    free (socket_path);
    return ret;
}

static void
test_trace (test_runner_t *test, const char *trace)
{
    char *trace_cpy, *name, *dot;

    trace_cpy = xstrdup (trace);
    name = basename (trace_cpy);
    dot = strchr (name, '.');
    if (dot)
	*dot = '\0';

    if (test->list_only) {
	printf ("%s\n", name);
    } else {
	cairo_bool_t ret;

	printf ("%s: ", name);
	fflush (stdout);

	ret = _test_trace (test, trace, name);
	printf ("%s\n", ret ? "PASS" : "FAIL");
    }

    free (trace_cpy);
}

#ifndef __USE_GNU
#define POORMANS_GETLINE_BUFFER_SIZE (65536)
static ssize_t
getline (char **lineptr, size_t *n, FILE *stream)
{
    if (*lineptr == NULL) {
        *n = POORMANS_GETLINE_BUFFER_SIZE;
        *lineptr = (char *) malloc (*n);
    }

    if (! fgets (*lineptr, *n, stream))
        return -1;

    if (! feof (stream) && !strchr (*lineptr, '\n')) {
        fprintf (stderr, "The poor man's implementation of getline in "
                          __FILE__ " needs a bigger buffer. Perhaps it's "
                         "time for a complete implementation of getline.\n");
        exit (0);
    }

    return strlen (*lineptr);
}
#undef POORMANS_GETLINE_BUFFER_SIZE

static char *
strndup (const char *s, size_t n)
{
    size_t len;
    char *sdup;

    if (!s)
        return NULL;

    len = strlen (s);
    len = (n < len ? n : len);
    sdup = (char *) malloc (len + 1);
    if (sdup)
    {
        memcpy (sdup, s, len);
        sdup[len] = '\0';
    }

    return sdup;
}
#endif /* ifndef __USE_GNU */

static cairo_bool_t
read_excludes (test_runner_t *test, const char *filename)
{
    FILE *file;
    char *line = NULL;
    size_t line_size = 0;
    char *s, *t;

    file = fopen (filename, "r");
    if (file == NULL)
	return FALSE;

    while (getline (&line, &line_size, file) != -1) {
	/* terminate the line at a comment marker '#' */
	s = strchr (line, '#');
	if (s)
	    *s = '\0';

	/* whitespace delimits */
	s = line;
	while (*s != '\0' && isspace (*s))
	    s++;

	t = s;
	while (*t != '\0' && ! isspace (*t))
	    t++;

	if (s != t) {
	    int i = test->num_exclude_names;
	    test->exclude_names = xrealloc (test->exclude_names,
					    sizeof (char *) * (i+1));
	    test->exclude_names[i] = strndup (s, t-s);
	    test->num_exclude_names++;
	}
    }
    if (line != NULL)
	free (line);

    fclose (file);

    return TRUE;
}

static void
usage (const char *argv0)
{
    fprintf (stderr,
"Usage: %s [-x exclude-file] [test-names ... | traces ...]\n"
"       %s -l\n"
"\n"
"Run the cairo test suite over the given traces (all by default).\n"
"The command-line arguments are interpreted as follows:\n"
"\n"
"  -x   exclude; specify a file to read a list of traces to exclude\n"
"  -l	list only; just list selected test case names without executing\n"
"\n"
"If test names are given they are used as sub-string matches so a command\n"
"such as \"cairo-test-trace firefox\" can be used to run all firefox traces.\n"
"Alternatively, you can specify a list of filenames to execute.\n",
	     argv0, argv0);
}

static void
parse_options (test_runner_t *test, int argc, char *argv[])
{
    int c;

    test->list_only = FALSE;
    test->names = NULL;
    test->num_names = 0;
    test->exclude_names = NULL;
    test->num_exclude_names = 0;

    while (1) {
	c = _cairo_getopt (argc, argv, "x:l");
	if (c == -1)
	    break;

	switch (c) {
	case 'l':
	    test->list_only = TRUE;
	    break;
	case 'x':
	    if (! read_excludes (test, optarg)) {
		fprintf (stderr, "Invalid argument for -x (not readable file): %s\n",
			 optarg);
		exit (1);
	    }
	    break;
	default:
	    fprintf (stderr, "Internal error: unhandled option: %c\n", c);
	    /* fall-through */
	case '?':
	    usage (argv[0]);
	    exit (1);
	}
    }

    if (optind < argc) {
	test->names = &argv[optind];
	test->num_names = argc - optind;
    }
}

static void
test_reset (test_runner_t *test)
{
    cairo_debug_reset_static_data ();
#if HAVE_FCFINI
    FcFini ();
#endif
}

static void
test_fini (test_runner_t *test)
{
    test_reset (test);

    cairo_boilerplate_free_targets (test->targets);
    if (test->exclude_names)
	free (test->exclude_names);
}

static cairo_bool_t
test_has_filenames (test_runner_t *test)
{
    unsigned int i;

    if (test->num_names == 0)
	return FALSE;

    for (i = 0; i < test->num_names; i++)
	if (access (test->names[i], R_OK) == 0)
	    return TRUE;

    return FALSE;
}

static cairo_bool_t
test_can_run (test_runner_t *test, const char *name)
{
    unsigned int i;
    char *copy, *dot;
    cairo_bool_t ret;

    if (test->num_names == 0 && test->num_exclude_names == 0)
	return TRUE;

    copy = xstrdup (name);
    dot = strrchr (copy, '.');
    if (dot != NULL)
	*dot = '\0';

    if (test->num_names) {
	ret = TRUE;
	for (i = 0; i < test->num_names; i++)
	    if (strstr (copy, test->names[i]))
		goto check_exclude;

	ret = FALSE;
	goto done;
    }

check_exclude:
    if (test->num_exclude_names) {
	ret = FALSE;
	for (i = 0; i < test->num_exclude_names; i++)
	    if (strstr (copy, test->exclude_names[i]))
		goto done;

	ret = TRUE;
	goto done;
    }

done:
    free (copy);

    return ret;
}

static void
warn_no_traces (const char *message, const char *trace_dir)
{
    fprintf (stderr,
"Error: %s '%s'.\n"
"Have you cloned the cairo-traces repository and uncompressed the traces?\n"
"  git clone git://anongit.freedesktop.org/cairo-traces\n"
"  cd cairo-traces && make\n"
"Or set the env.var CAIRO_TRACE_DIR to point to your traces?\n",
	    message, trace_dir);
}

static void
interrupt (int sig)
{
    shm_unlink (SHM_PATH_XXX);

    signal (sig, SIG_DFL);
    raise (sig);
}

int
main (int argc, char *argv[])
{
    test_runner_t test;
    const char *trace_dir = "cairo-traces";
    unsigned int n;

    signal (SIGPIPE, SIG_IGN);
    signal (SIGINT, interrupt);

    parse_options (&test, argc, argv);

    if (getenv ("CAIRO_TRACE_DIR") != NULL)
	trace_dir = getenv ("CAIRO_TRACE_DIR");

    test.targets = cairo_boilerplate_get_targets (&test.num_targets, NULL);

    if (test_has_filenames (&test)) {
	for (n = 0; n < test.num_names; n++) {
	    if (test_can_run (&test, test.names[n]) &&
		access (test.names[n], R_OK) == 0)
	    {
		test_trace (&test, test.names[n]);
		test_reset (&test);
	    }
	}
    } else {
	DIR *dir;
	struct dirent *de;
	int num_traces = 0;

	dir = opendir (trace_dir);
	if (dir == NULL) {
	    warn_no_traces ("Failed to open directory", trace_dir);
	    test_fini (&test);
	    return 1;
	}

	while ((de = readdir (dir)) != NULL) {
	    char *trace;
	    const char *dot;

	    dot = strrchr (de->d_name, '.');
	    if (dot == NULL)
		continue;
	    if (strcmp (dot, ".trace"))
		continue;

	    num_traces++;
	    if (! test_can_run (&test, de->d_name))
		continue;

	    xasprintf (&trace, "%s/%s", trace_dir, de->d_name);
	    test_trace (&test, trace);
	    test_reset (&test);

	    free (trace);

	}
	closedir (dir);

	if (num_traces == 0) {
	    warn_no_traces ("Found no traces in", trace_dir);
	    test_fini (&test);
	    return 1;
	}
    }

    test_fini (&test);

    return 0;
}
