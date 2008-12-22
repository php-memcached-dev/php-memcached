dnl
dnl $ Id: $
dnl vim:se ts=2 sw=2 et:

PHP_ARG_ENABLE(memcached, whether to enable memcached support,
[  --enable-memcached       Enable memcached support])

PHP_ARG_WITH(libmemcached-dir,  for libmemcached,
[  --with-libmemcached-dir[=DIR]   Set the path to libmemcached install prefix.], yes)

if test -z "$PHP_ZLIB_DIR"; then
PHP_ARG_WITH(zlib-dir, for ZLIB,
[  --with-zlib-dir[=DIR]   Set the path to ZLIB install prefix.], no)
fi

if test -z "$PHP_DEBUG"; then
  AC_ARG_ENABLE(debug,
  [  --enable-debug          compile with debugging symbols],[
    PHP_DEBUG=$enableval
  ],[    PHP_DEBUG=no
  ])
fi

if test "$PHP_MEMCACHED" != "no"; then

  dnl # zlib
  if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
    if test -f "$PHP_ZLIB_DIR/include/zlib/zlib.h"; then
      PHP_ZLIB_DIR="$PHP_ZLIB_DIR"
      PHP_ZLIB_INCDIR="$PHP_ZLIB_DIR/include/zlib"
    elif test -f "$PHP_ZLIB_DIR/include/zlib.h"; then
      PHP_ZLIB_DIR="$PHP_ZLIB_DIR"
      PHP_ZLIB_INCDIR="$PHP_ZLIB_DIR/include"
    else
      AC_MSG_ERROR([Can't find ZLIB headers under "$PHP_ZLIB_DIR"])
    fi
  else
    for i in /usr/local /usr; do
      if test -f "$i/include/zlib/zlib.h"; then
        PHP_ZLIB_DIR="$i"
        PHP_ZLIB_INCDIR="$i/include/zlib"
      elif test -f "$i/include/zlib.h"; then
        PHP_ZLIB_DIR="$i"
        PHP_ZLIB_INCDIR="$i/include"
      fi
    done
  fi

  AC_MSG_CHECKING([for zlib location])
  if test "$PHP_ZLIB_DIR" = "no"; then
    AC_MSG_ERROR([memcached support requires ZLIB. Use --with-zlib-dir=<DIR> to specify the prefix where ZLIB headers and library are located])
  else
    AC_MSG_RESULT([$PHP_ZLIB_DIR])
    PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/$PHP_LIBDIR, MEMCACHE_SHARED_LIBADD)
    PHP_ADD_INCLUDE($PHP_ZLIB_INCDIR)
  fi

  if test "$PHP_LIBMEMCACHED_DIR" != "no" && test "$PHP_LIBMEMCACHED_DIR" != "yes"; then
    if test -r "$PHP_LIBMEMCACHED_DIR/include/libmemcached/memcached.h"; then
      PHP_LIBMEMCACHED_DIR="$PHP_LIBMEMCACHED_DIR"
    else
      AC_MSG_ERROR([Can't find libmemcached headers under "$PHP_LIBMEMCACHED_DIR"])
    fi
  else
    for i in /usr /usr/local; do
	    if test -r "$i/include/libmemcached/memcached.h"; then
	  	  PHP_LIBMEMCACHED_DIR=$i
	  	  break
	    fi
	  done
  fi

  AC_MSG_CHECKING([for libmemcached location])
  if test "$PHP_LIBMEMCACHED_DIR" = "no"; then
    AC_MSG_ERROR([memcached support requires libmemcached. Use --with-libmemcached-dir=<DIR> to specify the prefix where libmemcached headers and library are located])
  else
    AC_MSG_RESULT([$PHP_LIBMEMCACHED_DIR])
    PHP_LIBMEMCACHED_INCDIR="$PHP_LIBMEMCACHED_DIR/include"
    PHP_ADD_INCLUDE($PHP_LIBMEMCACHED_INCDIR)
    PHP_ADD_LIBRARY_WITH_PATH(memcached, $PHP_LIBMEMCACHED_DIR/lib, MEMCACHED_SHARED_LIBADD)

    PHP_SUBST(MEMCACHED_SHARED_LIBADD)
    PHP_NEW_EXTENSION(memcached, php_memcached.c , $ext_shared)

    ifdef([PHP_ADD_EXTENSION_DEP],
    [
      PHP_ADD_EXTENSION_DEP(pdo, spl, true)
    ])

  fi

fi

