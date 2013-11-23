/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2009 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Andrei Zmievski <andrei@php.net>                            |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_LIBMEMCACHED_COMPAT
#define PHP_LIBMEMCACHED_COMPAT

/* this is the version(s) we support */
#include <libmemcached/memcached.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

memcached_st *php_memc_create_str (const char *str, size_t str_len);

#ifndef HAVE_LIBMEMCACHED_SERVER_TEMPORARILY_MARKER_DISABLED
#  define MEMCACHED_SERVER_TEMPORARILY_DISABLED (1024 << 2)
#endif

#ifdef HAVE_MEMCACHED_INSTANCE_ST
typedef const memcached_instance_st * php_memcached_instance_st;
#else
typedef memcached_server_instance_st php_memcached_instance_st;
#endif

#endif
