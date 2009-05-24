/*
 * Copyright © 2009 Joonas Pihlaja
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
/* Usage:
 *   ./make-cairo-test-constructors [tests.c...] >cairo-test-constructors.c
 *
 * Parses invocations of the CAIRO_TEST macro from the source files
 * given on the command line, gathers names of tests, and outputs a C
 * file with one function _cairo_test_runner_register_tests() which
 * calls the functions _register_<testname>() in reverse order.
 */
/* Keep this file ANSI compliant without any special needs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct name {
    struct name *next;
    char        *name;
};

static struct name *head = NULL;

static void *
xmalloc (size_t n)
{
    void *bytes = malloc(n);
    if (!bytes) {
        fprintf (stderr, "Out of memory\n");
        exit(2);
    }
    return bytes;
}

static void
add_name (const char *name)
{
    struct name *node = xmalloc (sizeof (struct name));

    node->name = xmalloc (strlen(name)+1);
    strcpy (node->name, name);

    node->next = head;
    head = node;
}

static int
scan_file (const char   *filename,
           FILE         *fp)
{
    int line_num = 0;
    char linebuf[1024];
    int fail = 0;

    while (fgets (linebuf, sizeof (linebuf)-1, fp)) {
        char *macro;
        char *name;
        size_t length;

        line_num++;
        linebuf[sizeof (linebuf)-1] = 0;

        macro = strstr (linebuf, "CAIRO_TEST");
        if (!macro)
            continue;
        macro += strlen ("CAIRO_TEST");

        length = strspn (macro, " (");
        if (length == 0)
            continue;
        name = macro + length;

        length = strspn (name,
                         "_"
                         "abcdefghijklmnopqrstuvwxyz"
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "0123456789");
        name[length] = 0;

        if (length == 0) {
            fprintf (stderr, "%s:%d: CAIRO_TEST invocation "
                     "can't be parsed by make-cairo-test-constructors.c\n",
                     filename, line_num);
            fail = 1;
            continue;
        }

        add_name (name);
    }

    return fail;
}

int
main (int argc, char **argv)
{
    int i;
    int fail = 0;
    struct name *node;

    for (i=1; i<argc; i++) {
        FILE *fp = fopen (argv[i], "r");
        if (fp) {
            fail |= scan_file (argv[i], fp);
            fclose (fp);
        }
    }
    if (fail)
        exit(1);

    puts ("/* WARNING: Autogenerated file - "
          "see make-cairo-test-constructors.c! */");
    puts ("");
    puts ("#include \"cairo-test-private.h\"");
    puts ("");

    for (node = head; node; node = node->next) {
        printf("extern void _register_%s (void);\n",
               node->name);
    }
    puts("");

    puts ("void _cairo_test_runner_register_tests (void);");
    puts("");

    puts ("void");
    puts ("_cairo_test_runner_register_tests (void)");
    puts ("{");
    for (node = head; node; node = node->next) {
        printf("    _register_%s ();\n", node->name);
    }
    puts ("}");

    return 0;
}
