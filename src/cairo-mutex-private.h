/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005,2007 Red Hat, Inc.
 * Copyright © 2007 Mathias Hasselmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Mathias Hasselmann <mathias.hasselmann@gmx.de>
 *	Behdad Esfahbod <behdad@behdad.org>
 */

#ifndef CAIRO_MUTEX_PRIVATE_H
#define CAIRO_MUTEX_PRIVATE_H

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <cairo-features.h>

#include "cairo-mutex-type-private.h"

/* Only the following three are mandatory at this point */
#ifndef CAIRO_MUTEX_LOCK
# error "CAIRO_MUTEX_LOCK not defined.  Check cairo-mutex-type-private.h."
#endif
#ifndef CAIRO_MUTEX_UNLOCK
# error "CAIRO_MUTEX_UNLOCK not defined.  Check cairo-mutex-type-private.h."
#endif
#ifndef CAIRO_MUTEX_NIL_INITIALIZER
# error "CAIRO_MUTEX_NIL_INITIALIZER not defined.  Check cairo-mutex-type-private.h."
#endif

CAIRO_BEGIN_DECLS

#define CAIRO_MUTEX_DECLARE(mutex) extern cairo_mutex_t mutex
#include "cairo-mutex-list-private.h"
#undef CAIRO_MUTEX_DECLARE


#ifndef CAIRO_MUTEX_INIT
# define CAIRO_MUTEX_INIT(mutex) do {				\
    cairo_mutex_t _tmp_mutex = CAIRO_MUTEX_NIL_INITIALIZER;     \
    memcpy (&(mutex), &_tmp_mutex, sizeof (_tmp_mutex));        \
} while (0)
#endif

#ifndef CAIRO_MUTEX_FINI
# define CAIRO_MUTEX_FINI(mutex)	CAIRO_MUTEX_NOOP1(mutex)
#endif


#ifndef CAIRO_MUTEX_INITIALIZE
# define CAIRO_MUTEX_USE_GENERIC_INITIALIZATION 1
#else
# undef CAIRO_MUTEX_USE_GENERIC_INITIALIZATION
# ifndef CAIRO_MUTEX_FINALIZE
#  define CAIRO_MUTEX_FINALIZE() CAIRO_MUTEX_NOOP
# endif
#endif

#if CAIRO_MUTEX_USE_GENERIC_INITIALIZATION

#define CAIRO_MUTEX_INITIALIZE() do { \
    if (!_cairo_mutex_initialized) \
        _cairo_mutex_initialize (); \
} while(0)

#define CAIRO_MUTEX_FINALIZE() do { \
    if (_cairo_mutex_initialized) \
        _cairo_mutex_finalize (); \
} while(0)

cairo_private extern cairo_bool_t _cairo_mutex_initialized;
cairo_private void _cairo_mutex_initialize(void);
cairo_private void _cairo_mutex_finalize(void);

#endif

CAIRO_END_DECLS

/* Make sure everything we want is defined */
#ifndef CAIRO_MUTEX_INITIALIZE
# error "CAIRO_MUTEX_INITIALIZE not defined"
#endif
#ifndef CAIRO_MUTEX_FINALIZE
# error "CAIRO_MUTEX_FINALIZE not defined"
#endif
#ifndef CAIRO_MUTEX_LOCK
# error "CAIRO_MUTEX_LOCK not defined"
#endif
#ifndef CAIRO_MUTEX_UNLOCK
# error "CAIRO_MUTEX_UNLOCK not defined"
#endif
#ifndef CAIRO_MUTEX_INIT
# error "CAIRO_MUTEX_INIT not defined"
#endif
#ifndef CAIRO_MUTEX_FINI
# error "CAIRO_MUTEX_FINI not defined"
#endif
#ifndef CAIRO_MUTEX_NIL_INITIALIZER
# error "CAIRO_MUTEX_NIL_INITIALIZER not defined"
#endif

#endif
