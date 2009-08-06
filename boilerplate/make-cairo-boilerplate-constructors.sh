#!/bin/sh

cat <<HERE
/* WARNING: Autogenerated file - see $0! */

#include "cairo-boilerplate-private.h"

void _cairo_boilerplate_register_all (void);

HERE

cat "$@" |  sed '/^CAIRO_BOILERPLATE/!d; s/CAIRO_BOILERPLATE.*(\(.*\),.*/extern void _register_\1 (void);/'

cat <<HERE

void
_cairo_boilerplate_register_all (void)
{
HERE

cat "$@" |  sed '/^CAIRO_BOILERPLATE/!d; s/CAIRO_BOILERPLATE.*(\(.*\),.*/    _register_\1 ();/'

echo "}"

