/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2009-2010 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Andrei Zmievski <andrei@php.net>                            |
  +----------------------------------------------------------------------+
*/

#include <stdlib.h>
#include <string.h>
#include <php.h>
#include <php_main.h>

#ifdef ZTS
#include "TSRM.h"
#endif

#include <php_ini.h>
#include <SAPI.h>
#include <ext/standard/info.h>
#include <zend_extensions.h>
#include <zend_exceptions.h>
#include <ext/standard/php_smart_str.h>
#include <ext/standard/php_var.h>
#include <libmemcached/memcached.h>

#include "php_memcached.h"
#include "php_memcached_session.h"

extern ZEND_DECLARE_MODULE_GLOBALS(php_memcached)

#define MEMC_SESS_DEFAULT_LOCK_WAIT 150000
#define MEMC_SESS_LOCK_EXPIRATION 30

ps_module ps_mod_memcached = {
	PS_MOD(memcached)
};

static int php_memc_sess_lock(memcached_st *memc, const char *key TSRMLS_DC)
{
	char *lock_key = NULL;
	int lock_key_len = 0;
	long attempts;
	long lock_maxwait;
	long lock_wait = MEMC_G(sess_lock_wait);
	time_t expiration;
	memcached_return status;
	/* set max timeout for session_start = max_execution_time.  (c) Andrei Darashenka, Richter & Poweleit GmbH */

	lock_maxwait = zend_ini_long(ZEND_STRS("max_execution_time"), 0);
	if (lock_maxwait <= 0) {
		lock_maxwait = MEMC_SESS_LOCK_EXPIRATION;
	}
	if (lock_wait == 0) {
		lock_wait = MEMC_SESS_DEFAULT_LOCK_WAIT;
	}
	expiration  = time(NULL) + lock_maxwait + 1;
	attempts = lock_maxwait * 1000000 / lock_wait;

	lock_key_len = spprintf(&lock_key, 0, "lock.%s", key);
	do {
		status = memcached_add(memc, lock_key, lock_key_len, "1", sizeof("1")-1, expiration, 0);
		if (status == MEMCACHED_SUCCESS) {
			MEMC_G(sess_locked) = 1;
			MEMC_G(sess_lock_key) = lock_key;
			MEMC_G(sess_lock_key_len) = lock_key_len;
			return 0;
		} else if (status != MEMCACHED_NOTSTORED) {
			break;
		}

		if (lock_wait > 0) {
			usleep(lock_wait);
		}
	} while(--attempts > 0);

	efree(lock_key);
	return -1;
}

static void php_memc_sess_unlock(memcached_st *memc TSRMLS_DC)
{
	if (MEMC_G(sess_locked)) {
		memcached_delete(memc, MEMC_G(sess_lock_key), MEMC_G(sess_lock_key_len), 0);
		MEMC_G(sess_locked) = 0;
		efree(MEMC_G(sess_lock_key));
		MEMC_G(sess_lock_key_len) = 0;
	}
}

PS_OPEN_FUNC(memcached)
{
	memcached_st *memc_sess = PS_GET_MOD_DATA();
	memcached_server_st *servers;
	memcached_return status;

	servers = memcached_servers_parse((char *)save_path);
	if (servers) {
		memc_sess = memcached_create(NULL);
		if (memc_sess) {
			status = memcached_server_push(memc_sess, servers);
			memcached_server_list_free(servers);

			if (memcached_callback_set(memc_sess, MEMCACHED_CALLBACK_PREFIX_KEY, MEMC_G(sess_prefix)) != MEMCACHED_SUCCESS) {
				PS_SET_MOD_DATA(NULL);
				memcached_free(memc_sess);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "bad memcached key prefix in memcached.sess_prefix");
				return FAILURE;
			}

			if (status == MEMCACHED_SUCCESS) {
				PS_SET_MOD_DATA(memc_sess);
				return SUCCESS;
			}
		} else {
			memcached_server_list_free(servers);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not allocate libmemcached structure");
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to parse session.save_path");
	}

	PS_SET_MOD_DATA(NULL);
	return FAILURE;
}

PS_CLOSE_FUNC(memcached)
{
	memcached_st *memc_sess = PS_GET_MOD_DATA();

	if (MEMC_G(sess_locking_enabled)) {
		php_memc_sess_unlock(memc_sess TSRMLS_CC);
	}
	if (memc_sess) {
		memcached_free(memc_sess);
		PS_SET_MOD_DATA(NULL);
	}

	return SUCCESS;
}

PS_READ_FUNC(memcached)
{
	char *payload = NULL;
	size_t payload_len = 0;
	int key_len = strlen(key);
	uint32_t flags = 0;
	memcached_return status;
	memcached_st *memc_sess = PS_GET_MOD_DATA();
	size_t key_length;

	key_length = strlen(MEMC_G(sess_prefix)) + key_len + 5; // prefix + "lock."
	if (!key_length || key_length >= MEMCACHED_MAX_KEY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The session id is too long or contains illegal characters");
		PS(invalid_session_id) = 1;
		return FAILURE;
	}

	if (MEMC_G(sess_locking_enabled)) {
		if (php_memc_sess_lock(memc_sess, key TSRMLS_CC) < 0) {
			return FAILURE;
		}
	}

	payload = memcached_get(memc_sess, key, key_len, &payload_len, &flags, &status);

	if (status == MEMCACHED_SUCCESS) {
		*val = estrndup(payload, payload_len);
		*vallen = payload_len;
		free(payload);
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

PS_WRITE_FUNC(memcached)
{
	int key_len = strlen(key);
	time_t expiration = 0;
	memcached_return status;
	memcached_st *memc_sess = PS_GET_MOD_DATA();
	size_t key_length;

	key_length = strlen(MEMC_G(sess_prefix)) + key_len + 5; // prefix + "lock."
	if (!key_length || key_length >= MEMCACHED_MAX_KEY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The session id is too long or contains illegal characters");
		PS(invalid_session_id) = 1;
		return FAILURE;
	}

	if (PS(gc_maxlifetime) > 0) {
		expiration = PS(gc_maxlifetime);
	}
	status = memcached_set(memc_sess, key, key_len, val, vallen, expiration, 0);

	if (status == MEMCACHED_SUCCESS) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

PS_DESTROY_FUNC(memcached)
{
	memcached_st *memc_sess = PS_GET_MOD_DATA();

	memcached_delete(memc_sess, key, strlen(key), 0);
	if (MEMC_G(sess_locking_enabled)) {
		php_memc_sess_unlock(memc_sess TSRMLS_CC);
	}

	return SUCCESS;
}

PS_GC_FUNC(memcached)
{
	return SUCCESS;
}
/* }}} */
