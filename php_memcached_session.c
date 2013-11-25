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

#include "php_memcached.h"
#include "php_memcached_private.h"
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
	unsigned long attempts;
	long write_retry_attempts = 0;
	long lock_maxwait = MEMC_G(sess_lock_max_wait);
	long lock_wait = MEMC_G(sess_lock_wait);
	long lock_expire = MEMC_G(sess_lock_expire);
	time_t expiration;
	memcached_return status;
	/* set max timeout for session_start = max_execution_time.  (c) Andrei Darashenka, Richter & Poweleit GmbH */
	if (lock_maxwait <= 0) {
		lock_maxwait = zend_ini_long(ZEND_STRS("max_execution_time"), 0);
		if (lock_maxwait <= 0) {
			lock_maxwait = MEMC_SESS_LOCK_EXPIRATION;
		}
	}
	if (lock_wait == 0) {
		lock_wait = MEMC_SESS_DEFAULT_LOCK_WAIT;
	}
	if (lock_expire <= 0) {
		lock_expire = lock_maxwait;
	}
	expiration  = time(NULL) + lock_expire + 1;
	attempts = (unsigned long)((1000000.0 / lock_wait) * lock_maxwait);

	/* Set the number of write retry attempts to the number of replicas times the number of attempts to remove a server */
	if (MEMC_G(sess_remove_failed_enabled)) {
		write_retry_attempts = MEMC_G(sess_number_of_replicas) * ( memcached_behavior_get(memc, MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT) + 1);
	}

	lock_key_len = spprintf(&lock_key, 0, "lock.%s", key);
	do {
		status = memcached_add(memc, lock_key, lock_key_len, "1", sizeof("1")-1, expiration, 0);
		if (status == MEMCACHED_SUCCESS) {
			MEMC_G(sess_locked) = 1;
			MEMC_G(sess_lock_key) = lock_key;
			MEMC_G(sess_lock_key_len) = lock_key_len;
			return 0;
		} else if (status != MEMCACHED_NOTSTORED && status != MEMCACHED_DATA_EXISTS) {
			if (write_retry_attempts > 0) {
				write_retry_attempts--;
				continue;
			}
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Write of lock failed");
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
	memcached_sess *memc_sess = PS_GET_MOD_DATA();
	memcached_return status;
	char *p, *plist_key = NULL;
	int plist_key_len = 0;

	if (!strncmp((char *)save_path, "PERSISTENT=", sizeof("PERSISTENT=") - 1)) {
		zend_rsrc_list_entry *le = NULL;
		char *e;

		p = (char *)save_path + sizeof("PERSISTENT=") - 1;
		if (!*p) {
error:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid persistent id for session storage");
			return FAILURE;
		}
		if ((e = strchr(p, ' '))) {
			plist_key_len = spprintf(&plist_key, 0, "memcached_sessions:id=%.*s", (int)(e - p), p);
		} else {
			goto error;
		}
		plist_key_len++;
		if (zend_hash_find(&EG(persistent_list), plist_key, plist_key_len, (void *)&le) == SUCCESS) {
			if (le->type == php_memc_sess_list_entry()) {
				memc_sess = (memcached_sess *) le->ptr;
				PS_SET_MOD_DATA(memc_sess);
				return SUCCESS;
			}
		}
		p = e + 1;
		memc_sess = pecalloc(sizeof(*memc_sess), 1, 1);
		memc_sess->is_persistent = 1;
	} else {
		p = (char *)save_path;
		memc_sess = ecalloc(sizeof(*memc_sess), 1);
		memc_sess->is_persistent = 0;
	}

	if (!strstr(p, "--SERVER")) {
		memcached_server_st *servers = memcached_servers_parse(p);
		if (servers) {
			memc_sess->memc_sess = memcached_create(NULL);
			if (memc_sess->memc_sess) {
				if (MEMC_G(sess_consistent_hash_enabled)) {
					if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED, (uint64_t) 1) == MEMCACHED_FAILURE) {
						PS_SET_MOD_DATA(NULL);
						if (plist_key) {
							efree(plist_key);
						}
						memcached_free(memc_sess->memc_sess);
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to enable memcached consistent hashing");
						return FAILURE;
					}
				}

				status = memcached_server_push(memc_sess->memc_sess, servers);
				memcached_server_list_free(servers);

				if (MEMC_G(sess_prefix) && MEMC_G(sess_prefix)[0] != 0 && memcached_callback_set(memc_sess->memc_sess, MEMCACHED_CALLBACK_PREFIX_KEY, MEMC_G(sess_prefix)) != MEMCACHED_SUCCESS) {
					PS_SET_MOD_DATA(NULL);
					if (plist_key) {
						efree(plist_key);
					}
					memcached_free(memc_sess->memc_sess);
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "bad memcached key prefix in memcached.sess_prefix");
					return FAILURE;
				}

				if (status == MEMCACHED_SUCCESS) {
					goto success;
				}
			} else {
				memcached_server_list_free(servers);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not allocate libmemcached structure");
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to parse session.save_path");
		}
	} else {
		memc_sess->memc_sess = php_memc_create_str(p, strlen(p));
		if (!memc_sess->memc_sess) {
#ifdef HAVE_LIBMEMCACHED_CHECK_CONFIGURATION
			char error_buffer[1024];
			if (libmemcached_check_configuration(p, strlen(p), error_buffer, sizeof(error_buffer)) != MEMCACHED_SUCCESS) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "session.save_path configuration error %s", error_buffer);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to initialize memcached session storage");
			}
#else
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to initialize memcached session storage");
#endif

		} else {
success:
			PS_SET_MOD_DATA(memc_sess);

			if (plist_key) {
				zend_rsrc_list_entry le;

				le.type = php_memc_sess_list_entry();
				le.ptr = memc_sess;

				if (zend_hash_update(&EG(persistent_list), (char *)plist_key, plist_key_len, (void *)&le, sizeof(le), NULL) == FAILURE) {
					efree(plist_key);
					php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not register persistent entry");
				}
				efree(plist_key);
			}

			if (MEMC_G(sess_binary_enabled)) {
				if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, (uint64_t) 1) == MEMCACHED_FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set memcached session binary protocol");
					return FAILURE;
				}
			}
#ifdef HAVE_MEMCACHED_SASL
			if (MEMC_G(use_sasl)) {
				/*
				* Enable SASL support if username and password are set
				*
				*/
				if (MEMC_G(sess_sasl_username) && MEMC_G(sess_sasl_password)) {
					/* Force binary protocol */
					if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, (uint64_t) 1) == MEMCACHED_FAILURE) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set memcached session binary protocol");
						return FAILURE;
					}
					if (memcached_set_sasl_auth_data(memc_sess->memc_sess, MEMC_G(sess_sasl_username), MEMC_G(sess_sasl_password)) == MEMCACHED_FAILURE) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set memcached session sasl credentials");
						return FAILURE;
					}
					MEMC_G(sess_sasl_data) = 1;
				}
			}


#endif
			if (MEMC_G(sess_number_of_replicas) > 0) {
				if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS, (uint64_t) MEMC_G(sess_number_of_replicas)) == MEMCACHED_FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set memcached session number of replicas");
					return FAILURE;
				}
				if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_RANDOMIZE_REPLICA_READ, (uint64_t) MEMC_G(sess_randomize_replica_read)) == MEMCACHED_FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set memcached session randomize replica read");
				}
			}

			if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, (uint64_t) MEMC_G(sess_connect_timeout)) == MEMCACHED_FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set memcached connection timeout");
				return FAILURE;
			}
#ifdef HAVE_MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS
			/* Allow libmemcached remove failed servers */
			if (MEMC_G(sess_remove_failed_enabled)) {
				if (memcached_behavior_set(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS, (uint64_t) 1) == MEMCACHED_FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to set: remove failed servers");
					return FAILURE;
				}
			}
#endif
			return SUCCESS;
		}
	}

	if (plist_key) {
		efree(plist_key);
	}
	PS_SET_MOD_DATA(NULL);
	return FAILURE;
}

PS_CLOSE_FUNC(memcached)
{
	memcached_sess *memc_sess = PS_GET_MOD_DATA();

	if (MEMC_G(sess_locking_enabled)) {
		php_memc_sess_unlock(memc_sess->memc_sess TSRMLS_CC);
	}
	if (memc_sess->memc_sess) {
		if (!memc_sess->is_persistent) {
#ifdef HAVE_MEMCACHED_SASL
			if (MEMC_G(sess_sasl_data)) {
				memcached_destroy_sasl_auth_data(memc_sess->memc_sess);
			}
#endif
			memcached_free(memc_sess->memc_sess);
			efree(memc_sess);
		}
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
	memcached_sess *memc_sess = PS_GET_MOD_DATA();
	size_t key_length;

	key_length = strlen(MEMC_G(sess_prefix)) + key_len + 5; // prefix + "lock."
	if (!key_length || key_length >= MEMCACHED_MAX_KEY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The session id is too long or contains illegal characters");
		PS(invalid_session_id) = 1;
		return FAILURE;
	}

	if (MEMC_G(sess_locking_enabled)) {
		if (php_memc_sess_lock(memc_sess->memc_sess, key TSRMLS_CC) < 0) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to clear session lock record");
			return FAILURE;
		}
	}

	payload = memcached_get(memc_sess->memc_sess, key, key_len, &payload_len, &flags, &status);

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
	long write_try_attempts = 1;
	memcached_return status;
	memcached_sess *memc_sess = PS_GET_MOD_DATA();
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

	/* Set the number of write retry attempts to the number of replicas times the number of attempts to remove a server plus the initial write */
	if (MEMC_G(sess_remove_failed_enabled)) {
		write_try_attempts = 1 + MEMC_G(sess_number_of_replicas) * ( memcached_behavior_get(memc_sess->memc_sess, MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT) + 1);
	}

	do {
		status = memcached_set(memc_sess->memc_sess, key, key_len, val, vallen, expiration, 0);
		if (status == MEMCACHED_SUCCESS) {
			return SUCCESS;
		} else {
			write_try_attempts--;
		}
	} while (write_try_attempts > 0);

	return FAILURE;
}

PS_DESTROY_FUNC(memcached)
{
	memcached_sess *memc_sess = PS_GET_MOD_DATA();

	memcached_delete(memc_sess->memc_sess, key, strlen(key), 0);
	if (MEMC_G(sess_locking_enabled)) {
		php_memc_sess_unlock(memc_sess->memc_sess TSRMLS_CC);
	}

	return SUCCESS;
}

PS_GC_FUNC(memcached)
{
	return SUCCESS;
}
/* }}} */
