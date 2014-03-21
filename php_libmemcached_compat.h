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

memcached_st *php_memc_create_str (const char *str, size_t str_len);

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX < 0x00052000
#  define MEMCACHED_SERVER_TEMPORARILY_DISABLED (1024 << 2)
#endif

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000002
#  define HAVE_MEMCACHED_TOUCH 1
#endif

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000017
#  define HAVE_MEMCACHED_INSTANCE_ST 1
#endif

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00049000
#  define HAVE_LIBMEMCACHED_CHECK_CONFIGURATION 1
#endif

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000002
#  define HAVE_MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS 1
#endif

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00049000
#  define HAVE_LIBMEMCACHED_MEMCACHED 1
#endif

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000018
#  define HAVE_MEMCACHED_BEHAVIOR_SERVER_TIMEOUT_LIMIT 1
#endif

#ifdef HAVE_MEMCACHED_INSTANCE_ST
typedef const memcached_instance_st * php_memcached_instance_st;
#else
typedef memcached_server_instance_st php_memcached_instance_st;
#endif

#endif
