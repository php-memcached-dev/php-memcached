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

memcached_return php_memcached_exist (memcached_st *memc, zend_string *key)
{
#ifdef HAVE_MEMCACHED_EXIST
	return memcached_exist (memc, key->val, key->len);
#else
	memcached_return rc = MEMCACHED_SUCCESS;
	uint32_t flags = 0;
	size_t value_length = 0;
	char *value = NULL;

	value = memcached_get (memc, key->val, key->len, &value_length, &flags, &rc);
	if (value) {
		free (value);
	}
	return rc;
#endif
}
