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

memcached_return php_memcached_exist (memcached_st *memc, zend_string *key);

memcached_return php_memcached_touch(memcached_st *memc, const char *key, size_t key_len, time_t expiration);
memcached_return php_memcached_touch_by_key(memcached_st *memc, const char *server_key, size_t server_key_len, const char *key, size_t key_len, time_t expiration);

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000017
typedef const memcached_instance_st * php_memcached_instance_st;
#else
typedef memcached_server_instance_st php_memcached_instance_st;
#endif

#endif
