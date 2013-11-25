dnl
dnl $ Id: $
dnl vim:se ts=2 sw=2 et:

PHP_ARG_ENABLE(memcached, whether to enable memcached support,
[  --enable-memcached               Enable memcached support])

PHP_ARG_WITH(libmemcached-dir,  for libmemcached,
[  --with-libmemcached-dir[=DIR]   Set the path to libmemcached install prefix.], yes)

PHP_ARG_ENABLE(memcached-session, whether to enable memcached session handler support,
[  --disable-memcached-session      Disable memcached session handler support], yes, no)

PHP_ARG_ENABLE(memcached-igbinary, whether to enable memcached igbinary serializer support,
[  --enable-memcached-igbinary      Enable memcached igbinary serializer support], no, no)

PHP_ARG_ENABLE(memcached-json, whether to enable memcached json serializer support,
[  --enable-memcached-json          Enable memcached json serializer support], no, no)

PHP_ARG_ENABLE(memcached-msgpack, whether to enable memcached msgpack serializer support,
[  --enable-memcached-msgpack          Enable memcached msgpack serializer support], no, no)

PHP_ARG_ENABLE(memcached-sasl, whether to enable memcached sasl support,
[  --disable-memcached-sasl          Disable memcached sasl support], yes, no)

PHP_ARG_ENABLE(memcached-protocol, whether to enable memcached protocol support,
[  --enable-memcached-protocol          Enable memcached protocoll support], no, no)

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

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  if test "x$PKG_CONFIG" = "xno"; then
    AC_MSG_RESULT([pkg-config not found])
    AC_MSG_ERROR([Please reinstall the pkg-config distribution])
  fi

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
  if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
    AC_MSG_RESULT([$PHP_ZLIB_DIR])
    PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/$PHP_LIBDIR, MEMCACHED_SHARED_LIBADD)
    PHP_ADD_INCLUDE($PHP_ZLIB_INCDIR)
  else
    AC_MSG_ERROR([memcached support requires ZLIB. Use --with-zlib-dir=<DIR> to specify the prefix where ZLIB headers and library are located])
  fi

  if test "$PHP_MEMCACHED_SESSION" != "no"; then
    AC_MSG_CHECKING([for session includes])
    session_inc_path=""

    if test -f "$abs_srcdir/include/php/ext/session/php_session.h"; then
      session_inc_path="$abs_srcdir/include/php"
    elif test -f "$abs_srcdir/ext/session/php_session.h"; then
      session_inc_path="$abs_srcdir"
    elif test -f "$phpincludedir/ext/session/php_session.h"; then
      session_inc_path="$phpincludedir"
    else
      for i in php php4 php5 php6; do
        if test -f "$prefix/include/$i/ext/session/php_session.h"; then
          session_inc_path="$prefix/include/$i"
        fi
      done
    fi

    if test "$session_inc_path" = ""; then
      AC_MSG_ERROR([Cannot find php_session.h])
    else
      AC_MSG_RESULT([$session_inc_path])
    fi
  fi
  
  if test "$PHP_MEMCACHED_JSON" != "no"; then
    AC_MSG_CHECKING([for json includes])
    json_inc_path=""
    
    tmp_version=$PHP_VERSION
    if test -z "$tmp_version"; then
      if test -z "$PHP_CONFIG"; then
        AC_MSG_ERROR([php-config not found])
      fi
      PHP_MEMCACHED_VERSION_ORIG=`$PHP_CONFIG --version`;
    else
      PHP_MEMCACHED_VERSION_ORIG=$tmp_version
    fi

    if test -z $PHP_MEMCACHED_VERSION_ORIG; then
      AC_MSG_ERROR([failed to detect PHP version, please report])
    fi

    PHP_MEMCACHED_VERSION_MASK=`echo ${PHP_MEMCACHED_VERSION_ORIG} | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
    
    if test $PHP_MEMCACHED_VERSION_MASK -ge 5003000; then
      if test -f "$abs_srcdir/include/php/ext/json/php_json.h"; then
        json_inc_path="$abs_srcdir/include/php"
      elif test -f "$abs_srcdir/ext/json/php_json.h"; then
        json_inc_path="$abs_srcdir"
      elif test -f "$phpincludedir/ext/json/php_json.h"; then
        json_inc_path="$phpincludedir"
      else
        for i in php php4 php5 php6; do
          if test -f "$prefix/include/$i/ext/json/php_json.h"; then
            json_inc_path="$prefix/include/$i"
          fi
        done
      fi
      if test "$json_inc_path" = ""; then
        AC_MSG_ERROR([Cannot find php_json.h])
      else
        AC_DEFINE(HAVE_JSON_API,1,[Whether JSON API is available])
        AC_DEFINE(HAVE_JSON_API_5_3,1,[Whether JSON API for PHP 5.3 is available])
        AC_MSG_RESULT([$json_inc_path])
      fi
    elif test $PHP_MEMCACHED_VERSION_MASK -ge 5002009; then
      dnl Check JSON for PHP 5.2.9+
      if test -f "$abs_srcdir/include/php/ext/json/php_json.h"; then
        json_inc_path="$abs_srcdir/include/php"
      elif test -f "$abs_srcdir/ext/json/php_json.h"; then
        json_inc_path="$abs_srcdir"
      elif test -f "$phpincludedir/ext/json/php_json.h"; then
        json_inc_path="$phpincludedir"
      else
        for i in php php4 php5 php6; do
          if test -f "$prefix/include/$i/ext/json/php_json.h"; then
            json_inc_path="$prefix/include/$i"
          fi
        done
      fi
      if test "$json_inc_path" = ""; then
        AC_MSG_ERROR([Cannot find php_json.h])
      else
        AC_DEFINE(HAVE_JSON_API,1,[Whether JSON API is available])
        AC_DEFINE(HAVE_JSON_API_5_2,1,[Whether JSON API for PHP 5.2 is available])
        AC_MSG_RESULT([$json_inc_path])
      fi
    else 
      AC_MSG_RESULT([the PHP version does not support JSON serialization API])
    fi
  fi

  if test "$PHP_MEMCACHED_IGBINARY" != "no"; then
    AC_MSG_CHECKING([for igbinary includes])
    igbinary_inc_path=""

    if test -f "$abs_srcdir/include/php/ext/igbinary/igbinary.h"; then
      igbinary_inc_path="$abs_srcdir/include/php"
    elif test -f "$abs_srcdir/ext/igbinary/igbinary.h"; then
      igbinary_inc_path="$abs_srcdir"
    elif test -f "$phpincludedir/ext/session/igbinary.h"; then
      igbinary_inc_path="$phpincludedir"
    elif test -f "$phpincludedir/ext/igbinary/igbinary.h"; then
      igbinary_inc_path="$phpincludedir"
    else
      for i in php php4 php5 php6; do
        if test -f "$prefix/include/$i/ext/igbinary/igbinary.h"; then
          igbinary_inc_path="$prefix/include/$i"
        fi
      done
    fi

    if test "$igbinary_inc_path" = ""; then
      AC_MSG_ERROR([Cannot find igbinary.h])
    else
      AC_MSG_RESULT([$igbinary_inc_path])
    fi
  fi

  if test "$PHP_MEMCACHED_MSGPACK" != "no"; then
    AC_MSG_CHECKING([for msgpack includes])
    msgpack_inc_path=""

    if test -f "$abs_srcdir/include/php/ext/msgpack/php_msgpack.h"; then
      msgpack_inc_path="$abs_srcdir/include/php"
    elif test -f "$abs_srcdir/ext/msgpack/php_msgpack.h"; then
      msgpack_inc_path="$abs_srcdir"
    elif test -f "$phpincludedir/ext/session/php_msgpack.h"; then
      msgpack_inc_path="$phpincludedir"
    elif test -f "$phpincludedir/ext/msgpack/php_msgpack.h"; then
      msgpack_inc_path="$phpincludedir"
    else
      for i in php php4 php5 php6; do
        if test -f "$prefix/include/$i/ext/msgpack/php_msgpack.h"; then
          msgpack_inc_path="$prefix/include/$i"
        fi
      done
    fi

    if test "$msgpack_inc_path" = ""; then
      AC_MSG_ERROR([Cannot find php_msgpack.h])
    else
      AC_MSG_RESULT([$msgpack_inc_path])
    fi
  fi 

  AC_MSG_CHECKING([for memcached session support])
  if test "$PHP_MEMCACHED_SESSION" != "no"; then
    AC_MSG_RESULT([enabled])
    AC_DEFINE(HAVE_MEMCACHED_SESSION,1,[Whether memcache session handler is enabled])
    SESSION_INCLUDES="-I$session_inc_path"
    ifdef([PHP_ADD_EXTENSION_DEP],
    [
      PHP_ADD_EXTENSION_DEP(memcached, session)
    ])
  else
    SESSION_INCLUDES=""
    AC_MSG_RESULT([disabled])
  fi

  AC_MSG_CHECKING([for memcached igbinary support])
  if test "$PHP_MEMCACHED_IGBINARY" != "no"; then
    AC_MSG_RESULT([enabled])
    AC_DEFINE(HAVE_MEMCACHED_IGBINARY,1,[Whether memcache igbinary serializer is enabled])
    IGBINARY_INCLUDES="-I$igbinary_inc_path"
    ifdef([PHP_ADD_EXTENSION_DEP],
    [
      PHP_ADD_EXTENSION_DEP(memcached, igbinary)
    ])
  else
    IGBINARY_INCLUDES=""
    AC_MSG_RESULT([disabled])
  fi

  AC_MSG_CHECKING([for memcached msgpack support])
  if test "$PHP_MEMCACHED_MSGPACK" != "no"; then
    AC_MSG_RESULT([enabled])
    AC_DEFINE(HAVE_MEMCACHED_MSGPACK,1,[Whether memcache msgpack serializer is enabled])
    MSGPACK_INCLUDES="-I$msgpack_inc_path"
    ifdef([PHP_ADD_EXTENSION_DEP],
    [
      PHP_ADD_EXTENSION_DEP(memcached, msgpack)
    ])
  else
    MSGPACK_INCLUDES=""
    AC_MSG_RESULT([disabled])
  fi

  AC_MSG_CHECKING([for libmemcached location])
  export ORIG_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"

  if test "$PHP_LIBMEMCACHED_DIR" != "no" && test "$PHP_LIBMEMCACHED_DIR" != "yes"; then
    export PKG_CONFIG_PATH="$PHP_LIBMEMCACHED_DIR/$PHP_LIBDIR/pkgconfig"

    if test ! -f "$PHP_LIBMEMCACHED_DIR/include/libmemcached/memcached.h"; then
      AC_MSG_ERROR(Unable to find memcached.h under $PHP_LIBMEMCACHED_DIR)
    fi
  else
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/$PHP_LIBDIR/pkgconfig:/usr/$PHP_LIBDIR/pkgconfig:/opt/$PHP_LIBDIR/pkgconfig"
  fi

  if ! $PKG_CONFIG --exists libmemcached; then
    AC_MSG_ERROR([memcached support requires libmemcached. Use --with-libmemcached-dir=<DIR> to specify the prefix where libmemcached headers and library are located])
  else
    PHP_LIBMEMCACHED_VERSION=`$PKG_CONFIG libmemcached --modversion`
    PHP_LIBMEMCACHED_DIR=`$PKG_CONFIG libmemcached --variable=prefix`

    AC_MSG_RESULT([found version $PHP_LIBMEMCACHED_VERSION, under $PHP_LIBMEMCACHED_DIR])

    PHP_LIBMEMCACHED_LIBS=`$PKG_CONFIG libmemcached --libs`
    PHP_LIBMEMCACHED_INCLUDES=`$PKG_CONFIG libmemcached --cflags`

    PHP_EVAL_LIBLINE($PHP_LIBMEMCACHED_LIBS, MEMCACHED_SHARED_LIBADD)
    PHP_EVAL_INCLINE($PHP_LIBMEMCACHED_INCLUDES)

    #
    # Added -lpthread here because AC_TRY_LINK tests on CentOS 6 seem to fail with undefined reference to pthread_once
    #
    ORIG_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $INCLUDES"

    AC_MSG_CHECKING([whether to enable sasl support])
    if test "$PHP_MEMCACHED_SASL" != "no"; then
      AC_MSG_RESULT(yes)
      AC_CHECK_HEADERS([sasl/sasl.h], [ac_cv_have_memc_sasl_h="yes"], [ac_cv_have_memc_sasl_h="no"])

      if test "$ac_cv_have_memc_sasl_h" = "yes"; then

        AC_CACHE_CHECK([whether libmemcached supports sasl], ac_cv_memc_sasl_support, [
          AC_TRY_COMPILE(
            [ #include <libmemcached/memcached.h> ],
            [ 
            #if LIBMEMCACHED_WITH_SASL_SUPPORT
              /* yes */
            #else
            #  error "no sasl support"
            #endif
            ],
            [ ac_cv_memc_sasl_support="yes" ],
            [ ac_cv_memc_sasl_support="no" ]
          )
        ])

        if test "$ac_cv_memc_sasl_support" = "yes"; then
          PHP_CHECK_LIBRARY(sasl2, sasl_client_init, [PHP_ADD_LIBRARY(sasl2, 1, MEMCACHED_SHARED_LIBADD)])
          AC_DEFINE(HAVE_MEMCACHED_SASL, 1, [Have SASL support])
        else
          AC_MSG_ERROR([no, libmemcached sasl support is not enabled. Run configure with --disable-memcached-sasl to disable this check])
        fi
      else
        AC_MSG_ERROR([no, sasl.h is not available. Run configure with --disable-memcached-sasl to disable this check])
      fi
    else
      AC_MSG_RESULT([no])
    fi

    PHP_MEMCACHED_FILES="php_memcached.c php_libmemcached_compat.c fastlz/fastlz.c g_fmt.c"

    if test "$PHP_MEMCACHED_SESSION" != "no"; then
      PHP_MEMCACHED_FILES="${PHP_MEMCACHED_FILES} php_memcached_session.c"
    fi

    LIBEVENT_INCLUDES=""
    AC_MSG_CHECKING([for memcached protocol support])
    if test "$PHP_MEMCACHED_PROTOCOL" != "no"; then
      AC_MSG_RESULT([enabled])

      AC_CACHE_CHECK([whether libmemcachedprotocol is usable], ac_cv_have_libmemcachedprotocol, [
        AC_TRY_COMPILE(
          [ #include <libmemcachedprotocol-0.0/handler.h> ],
          [ memcached_binary_protocol_callback_st s_test_impl;
            s_test_impl.interface.v1.delete_object = 0;
          ],
          [ ac_cv_have_libmemcachedprotocol="yes" ],
          [ ac_cv_have_libmemcachedprotocol="no" ]
        )
      ])

      if test "$ac_cv_have_libmemcachedprotocol" != "yes"; then
        AC_MSG_ERROR([Cannot enable libmemcached protocol])
      fi

      PHP_ADD_LIBRARY_WITH_PATH(memcachedprotocol, $PHP_LIBMEMCACHED_DIR/$PHP_LIBDIR, MEMCACHED_SHARED_LIBADD)

      AC_MSG_CHECKING([for libevent])
      if $PKG_CONFIG --exists libevent; then
        PHP_MEMCACHED_LIBEVENT_VERSION=`$PKG_CONFIG libevent --modversion`
        PHP_MEMCACHED_LIBEVENT_PREFIX=`$PKG_CONFIG libevent --variable=prefix`

        AC_MSG_RESULT([found version $PHP_MEMCACHED_LIBEVENT_VERSION, under $PHP_MEMCACHED_LIBEVENT_PREFIX])
        LIBEVENT_LIBS=`$PKG_CONFIG libevent --libs`
        LIBEVENT_INCLUDES=`$PKG_CONFIG libevent --cflags`

        PHP_EVAL_LIBLINE($LIBEVENT_LIBS, MEMCACHED_SHARED_LIBADD)
        PHP_EVAL_INCLINE($LIBEVENT_INCLUDES)
      else
        AC_MSG_ERROR(Unable to find libevent installation)
      fi
      PHP_MEMCACHED_FILES="${PHP_MEMCACHED_FILES} php_memcached_server.c"
      AC_DEFINE(HAVE_MEMCACHED_PROTOCOL,1,[Whether memcached protocol is enabled])
    else
      AC_MSG_RESULT([disabled])
    fi

    CFLAGS="$ORIG_CFLAGS"

    export PKG_CONFIG_PATH="$ORIG_PKG_CONFIG_PATH"
    PHP_SUBST(MEMCACHED_SHARED_LIBADD)

    PHP_NEW_EXTENSION(memcached, $PHP_MEMCACHED_FILES, $ext_shared,,$SESSION_INCLUDES $IGBINARY_INCLUDES $LIBEVENT_INCLUDES $MSGPACK_INCLUDES)
    PHP_ADD_BUILD_DIR($ext_builddir/fastlz, 1)
 
    ifdef([PHP_ADD_EXTENSION_DEP],
    [
      PHP_ADD_EXTENSION_DEP(memcached, spl, true)
    ])
  fi
fi

