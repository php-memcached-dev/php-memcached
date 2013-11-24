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

#include "php_memcached.h"
#include "php_memcached_private.h"
#include "php_libmemcached_compat.h"

memcached_st *php_memc_create_str (const char *str, size_t str_len)
{
#ifdef HAVE_LIBMEMCACHED_MEMCACHED
	return memcached (str, str_len);
#else
	memcached_return rc;
	memcached_st *memc;
	memcached_server_st *servers;

	memc = memcached_create(NULL);

	if (!memc) {
		return NULL;
	}

	servers = memcached_servers_parse (str);

	if (!servers) {
		memcached_free (memc);
		return NULL;
	}

	rc = memcached_server_push (memc, servers);
	memcached_server_free (servers);

	if (rc != MEMCACHED_SUCCESS) {
		memcached_free (memc);
		return NULL;
	}
	return memc;
#endif
}