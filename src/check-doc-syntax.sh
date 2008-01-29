#!/bin/sh

LANG=C

test -z "$srcdir" && srcdir=.
status=0

echo Checking documentation blocks for missing decorators

FILES=`find "$srcdir" -name '*.h' -or -name '*.c' -or -name '*.cpp'`

enum_regexp='^\([/ ][*] .*[^%@]\)\<\(FALSE\|TRUE\|NULL\|CAIRO_[0-9A-Z_]*[^(0-9A-Z_]\)'
if grep "$enum_regexp" $FILES; then
	status=1
	echo Error: some macros in the docs are not prefixed by percent sign.
	echo Fix this by running the following sed command as many times as needed:
	echo "	sed -i 's@$enum_regexp@\\1%\\2@' *.h *.c *.cpp"
fi

type_regexp='^\([/ ][*] .*[^#]\)\<\(cairo[0-9a-z_]*_t\>\)'
if grep "$type_regexp" $FILES; then
	status=1
	echo Error: some type names in the docs are not prefixed by hash sign.
	echo Fix this by running the following sed command as many times as needed:
	echo "	sed -i 's@$type_regexp@\\1#\\2@' *.h *.c *.cpp"
fi

func_regexp='^\([/ ][*] .*[^#]\)\<\(cairo_[][<>/0-9a-z_]*\> \?[^][ <>(]\)'
if grep "$func_regexp" $FILES; then
	status=1
	echo Error: some function names in the docs are not followed by parantheses.
	echo Fix this by searching for the following regexp in the above files:
	echo "	'$func_regexp'"
fi

exit $status
