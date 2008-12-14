dnl
dnl $ Id: $
dnl

PHP_ARG_WITH(memcached, libmemcached,[  --with-memcached[=DIR] With memcached support])


if test "$PHP_MEMCACHED" != "no"; then


  if test -r "$PHP_MEMCACHED/include/libmemcached/memcached.h"; then
	PHP_MEMCACHED_DIR="$PHP_MEMCACHED"
  else
	AC_MSG_CHECKING(for memcached in default path)
	for i in /usr /usr/local; do
	  if test -r "$i/include/libmemcached/memcached.h"; then
		PHP_MEMCACHED_DIR=$i
		AC_MSG_RESULT(found in $i)
		break
	  fi
	done
	if test "x" = "x$PHP_MEMCACHED_DIR"; then
	  AC_MSG_ERROR(not found)
	fi
  fi


  export OLD_CPPFLAGS="$CPPFLAGS"
  export CPPFLAGS="$CPPFLAGS $INCLUDES -DHAVE_MEMCACHED"
  export CPPFLAGS="$OLD_CPPFLAGS"

  PHP_ADD_INCLUDE($PHP_MEMCACHED_DIR/include)
  export OLD_CPPFLAGS="$CPPFLAGS"
  export CPPFLAGS="$CPPFLAGS $INCLUDES -DHAVE_MEMCACHED"

  AC_MSG_CHECKING(PHP version)
  AC_TRY_COMPILE([#include <php_version.h>], [
#if PHP_VERSION_ID < 40000
#error  this extension requires at least PHP version 4.0.0
#endif
],
[AC_MSG_RESULT(ok)],
[AC_MSG_ERROR([need at least PHP 4.0.0])])

  AC_CHECK_HEADER([libmemcached/memcached.h], [], AC_MSG_ERROR('libmemcached/memcached.h' header not found))
  export CPPFLAGS="$OLD_CPPFLAGS"
  PHP_SUBST(MEMCACHED_SHARED_LIBADD)

  PHP_ADD_LIBRARY_WITH_PATH(memcached, $PHP_MEMCACHED_DIR/lib, MEMCACHED_SHARED_LIBADD)


  PHP_SUBST(MEMCACHED_SHARED_LIBADD)
  AC_DEFINE(HAVE_MEMCACHED, 1, [ ])

  PHP_NEW_EXTENSION(memcached, memcached.c , $ext_shared)

fi

