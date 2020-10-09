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

#include "Zend/zend_smart_str_public.h"

extern ZEND_DECLARE_MODULE_GLOBALS(php_memcached)

#define REALTIME_MAXDELTA 60*60*24*30

ps_module ps_mod_memcached = {
	PS_MOD_UPDATE_TIMESTAMP(memcached)
};

typedef struct  {
	zend_bool is_persistent;
	zend_bool has_sasl_data;
	zend_bool    is_locked;
	zend_string *lock_key;
} php_memcached_user_data;

#ifndef MIN
# define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
# define MAX(a,b) (((a)>(b))?(a):(b))
#endif

static
	int le_memc_sess;

static
int s_memc_sess_list_entry(void)
{
	return le_memc_sess;
}

static
void s_destroy_mod_data(memcached_st *memc)
{
	php_memcached_user_data *user_data = memcached_get_user_data(memc);

#ifdef HAVE_MEMCACHED_SASL
	if (user_data->has_sasl_data) {
		memcached_destroy_sasl_auth_data(memc);
	}
#endif

	memcached_free(memc);
	pefree(memc, user_data->is_persistent);
	pefree(user_data, user_data->is_persistent);
}

ZEND_RSRC_DTOR_FUNC(php_memc_sess_dtor)
{
	if (res->ptr) {
		s_destroy_mod_data((memcached_st *) res->ptr);
		res->ptr = NULL;
	}
}

int php_memc_session_minit(int module_number)
{
	le_memc_sess =
		zend_register_list_destructors_ex(NULL, php_memc_sess_dtor, "Memcached Sessions persistent connection", module_number);

	php_session_register_module(ps_memcached_ptr);
	return SUCCESS;
}

static
time_t s_adjust_expiration(zend_long expiration)
{
    if (expiration <= REALTIME_MAXDELTA) {
        return expiration;
    } else {
        return time(NULL) + expiration;
    }
}

static
time_t s_lock_expiration()
{
	if (MEMC_SESS_INI(lock_expiration) > 0) {
		return s_adjust_expiration(MEMC_SESS_INI(lock_expiration));
	}
	else {
		zend_long max_execution_time = zend_ini_long(ZEND_STRL("max_execution_time"), 0);
		if (max_execution_time > 0) {
			return s_adjust_expiration(max_execution_time);
		}
	}
	return 0;
}

static
time_t s_session_expiration(zend_long maxlifetime)
{
	if (maxlifetime > 0) {
		return s_adjust_expiration(maxlifetime);
	}
	return 0;
}

static
zend_bool s_lock_session(memcached_st *memc, zend_string *sid)
{
	memcached_return rc;
	char *lock_key;
	size_t lock_key_len;
	time_t expiration;
	zend_long wait_time, retries;
	php_memcached_user_data *user_data = memcached_get_user_data(memc);

	lock_key_len = spprintf(&lock_key, 0, "lock.%s", sid->val);
	expiration   = s_lock_expiration();

	wait_time = MEMC_SESS_INI(lock_wait_min);
	retries   = MEMC_SESS_INI(lock_retries);

	do {
		rc = memcached_add(memc, lock_key, lock_key_len, "1", sizeof ("1") - 1, expiration, 0);

		switch (rc) {

			case MEMCACHED_SUCCESS:
				user_data->lock_key  = zend_string_init(lock_key, lock_key_len, user_data->is_persistent);
				user_data->is_locked = 1;
			break;

			case MEMCACHED_NOTSTORED:
			case MEMCACHED_DATA_EXISTS:
				if (retries > 0) {
					usleep(wait_time * 1000);
					wait_time = MIN(MEMC_SESS_INI(lock_wait_max), wait_time * 2);
				}
			break;

			default:
				php_error_docref(NULL, E_WARNING, "Failed to write session lock: %s", memcached_strerror (memc, rc));
				break;
		}
	} while (!user_data->is_locked && retries-- > 0);

	efree(lock_key);
	return user_data->is_locked;
}

static
void s_unlock_session(memcached_st *memc)
{
	php_memcached_user_data *user_data = memcached_get_user_data(memc);

	if (user_data->is_locked) {
		memcached_delete(memc, user_data->lock_key->val, user_data->lock_key->len, 0);
		user_data->is_locked = 0;
		zend_string_release (user_data->lock_key);
	}
}

static
zend_bool s_configure_from_ini_values(memcached_st *memc, zend_bool silent)
{
/* This macro looks like a function but returns errors directly */
#define check_set_behavior(behavior, value) \
{ \
	int b = (behavior); \
	uint64_t v = (value); \
	if (v != memcached_behavior_get(memc, b)) { \
		memcached_return rc; \
		if ((rc = memcached_behavior_set(memc, b, v)) != MEMCACHED_SUCCESS) { \
			if (!silent) { \
				php_error_docref(NULL, E_WARNING, "failed to initialise session memcached configuration: %s", memcached_strerror(memc, rc)); \
			} \
			return 0; \
		} \
	} \
}

	if (MEMC_SESS_INI(binary_protocol_enabled)) {
		check_set_behavior(MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
		/* Also enable TCP_NODELAY when binary protocol is enabled */
		check_set_behavior(MEMCACHED_BEHAVIOR_TCP_NODELAY, 1);
	}

	if (MEMC_SESS_INI(consistent_hash_enabled)) {
		check_set_behavior(MEMC_SESS_INI(consistent_hash_type), 1);
	}

	if (MEMC_SESS_INI(server_failure_limit)) {
		check_set_behavior(MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT, MEMC_SESS_INI(server_failure_limit));
	}

	if (MEMC_SESS_INI(number_of_replicas)) {
		check_set_behavior(MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS, MEMC_SESS_INI(number_of_replicas));
	}

	if (MEMC_SESS_INI(randomize_replica_read_enabled)) {
		check_set_behavior(MEMCACHED_BEHAVIOR_RANDOMIZE_REPLICA_READ, 1);
	}

	if (MEMC_SESS_INI(remove_failed_servers_enabled)) {
		check_set_behavior(MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS, 1);
	}

	if (MEMC_SESS_INI(connect_timeout)) {
		check_set_behavior(MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, MEMC_SESS_INI(connect_timeout));
	}

	if (MEMC_SESS_STR_INI(prefix)) {
		memcached_callback_set(memc, MEMCACHED_CALLBACK_NAMESPACE, MEMC_SESS_STR_INI(prefix));
	}

	if (MEMC_SESS_STR_INI(sasl_username) && MEMC_SESS_STR_INI(sasl_password)) {
		php_memcached_user_data *user_data;

		if (!php_memc_init_sasl_if_needed()) {
			return 0;
		}

		check_set_behavior(MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);

		if (memcached_set_sasl_auth_data(memc, MEMC_SESS_STR_INI(sasl_username), MEMC_SESS_STR_INI(sasl_password)) == MEMCACHED_FAILURE) {
			php_error_docref(NULL, E_WARNING, "failed to set memcached session sasl credentials");
			return 0;
		}
		user_data = memcached_get_user_data(memc);
		user_data->has_sasl_data = 1;
	}

#undef check_set_behavior

	return 1;
}

static
void *s_pemalloc_fn(const memcached_st *memc, size_t size, void *context)
{
	zend_bool *is_persistent = memcached_get_user_data(memc);

	return
		pemalloc(size, *is_persistent);
}

static
void s_pefree_fn(const memcached_st *memc, void *mem, void *context)
{
	zend_bool *is_persistent = memcached_get_user_data(memc);

	return
		pefree(mem, *is_persistent);
}

static
void *s_perealloc_fn(const memcached_st *memc, void *mem, const size_t size, void *context)
{
	zend_bool *is_persistent = memcached_get_user_data(memc);

	return
		perealloc(mem, size, *is_persistent);
}

static
void *s_pecalloc_fn(const memcached_st *memc, size_t nelem, const size_t elsize, void *context)
{
	zend_bool *is_persistent = memcached_get_user_data(memc);

	return
		pecalloc(nelem, elsize, *is_persistent);
}


static
memcached_st *s_init_mod_data (const memcached_server_list_st servers, zend_bool is_persistent)
{
	void *buffer;
	php_memcached_user_data *user_data;
	memcached_st *memc;

	buffer = pecalloc(1, sizeof(memcached_st), is_persistent);
	memc   = memcached_create (buffer);

	if (!memc) {
		php_error_docref(NULL, E_ERROR, "failed to allocate memcached structure");
		/* not reached */
	}

	memcached_set_memory_allocators(memc, s_pemalloc_fn, s_pefree_fn, s_perealloc_fn, s_pecalloc_fn, NULL);

	user_data                = pecalloc(1, sizeof(php_memcached_user_data), is_persistent);
	user_data->is_persistent = is_persistent;
	user_data->has_sasl_data = 0;
	user_data->lock_key      = NULL;
	user_data->is_locked     = 0;

	memcached_set_user_data(memc, user_data);
	memcached_server_push (memc, servers);
	memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_VERIFY_KEY, 1);
	return memc;
}

PS_OPEN_FUNC(memcached)
{
	memcached_st *memc   = NULL;
	char *plist_key      = NULL;
	size_t plist_key_len = 0;

	memcached_server_list_st servers;

	// Fail on incompatible PERSISTENT identifier (removed in php-memcached 3.0)
	if (strstr(save_path, "PERSISTENT=")) {
		php_error_docref(NULL, E_WARNING, "failed to parse session.save_path: PERSISTENT is replaced by memcached.sess_persistent = On");
		PS_SET_MOD_DATA(NULL);
		return FAILURE;
	}

	// First parse servers
	servers = memcached_servers_parse(save_path);

	if (!servers) {
		php_error_docref(NULL, E_WARNING, "failed to parse session.save_path");
		PS_SET_MOD_DATA(NULL);
		return FAILURE;
	}

	if (MEMC_SESS_INI(persistent_enabled)) {
		zend_resource *le_p;

		plist_key_len = spprintf(&plist_key, 0, "memc-session:%s", save_path);

		if ((le_p = zend_hash_str_find_ptr(&EG(persistent_list), plist_key, plist_key_len)) != NULL) {
			if (le_p->type == s_memc_sess_list_entry()) {
				memc = (memcached_st *) le_p->ptr;

				if (!s_configure_from_ini_values(memc, 1)) {
					// Remove existing plist entry
					zend_hash_str_del(&EG(persistent_list), plist_key, plist_key_len);
					memc = NULL;
				}
				else {
					efree(plist_key);
					PS_SET_MOD_DATA(memc);
					memcached_server_list_free(servers);
					return SUCCESS;
				}
			}
		}
	}

	memc = s_init_mod_data(servers, MEMC_SESS_INI(persistent_enabled));
	memcached_server_list_free(servers);

	if (!s_configure_from_ini_values(memc, 0)) {
		if (plist_key) {
			efree(plist_key);
		}
		s_destroy_mod_data(memc);
		PS_SET_MOD_DATA(NULL);
		return FAILURE;
	}

	if (plist_key) {
		zend_resource le;

		le.type = s_memc_sess_list_entry();
		le.ptr  = memc;

		GC_SET_REFCOUNT(&le, 1);

		/* plist_key is not a persistent allocated key, thus we use str_update here */
		if (zend_hash_str_update_mem(&EG(persistent_list), plist_key, plist_key_len, &le, sizeof(le)) == NULL) {
			php_error_docref(NULL, E_ERROR, "Could not register persistent entry for the memcached session");
			/* not reached */
		}
		efree(plist_key);
	}
	PS_SET_MOD_DATA(memc);
	return SUCCESS;
}

PS_CLOSE_FUNC(memcached)
{
	php_memcached_user_data *user_data;
	memcached_st *memc = PS_GET_MOD_DATA();

	if (!memc) {
		php_error_docref(NULL, E_WARNING, "Session is not allocated, check session.save_path value");
		return FAILURE;
	}

	user_data = memcached_get_user_data(memc);

	if (user_data->is_locked) {
		s_unlock_session(memc);
	}

	if (!user_data->is_persistent) {
		s_destroy_mod_data(memc);
	}

	PS_SET_MOD_DATA(NULL);
	return SUCCESS;
}

PS_READ_FUNC(memcached)
{
	char *payload = NULL;
	size_t payload_len = 0;
	uint32_t flags = 0;
	memcached_return status;
	memcached_st *memc = PS_GET_MOD_DATA();

	if (!memc) {
		php_error_docref(NULL, E_WARNING, "Session is not allocated, check session.save_path value");
		return FAILURE;
	}

	if (MEMC_SESS_INI(lock_enabled)) {
		if (!s_lock_session(memc, key)) {
			php_error_docref(NULL, E_WARNING, "Unable to clear session lock record");
			return FAILURE;
		}
	}

	payload = memcached_get(memc, key->val, key->len, &payload_len, &flags, &status);

	if (status == MEMCACHED_SUCCESS) {
		zend_bool *is_persistent = memcached_get_user_data(memc);
		*val = zend_string_init(payload, payload_len, 0);
		pefree(payload, *is_persistent);
		return SUCCESS;
	} else if (status == MEMCACHED_NOTFOUND) {
		*val = ZSTR_EMPTY_ALLOC();
		return SUCCESS;
	} else {
		php_error_docref(NULL, E_WARNING, "error getting session from memcached: %s", memcached_last_error_message(memc));
		return FAILURE;
	}
}

PS_WRITE_FUNC(memcached)
{
	zend_long retries = 1;
	memcached_st *memc = PS_GET_MOD_DATA();
	time_t expiration = s_session_expiration(maxlifetime);

	if (!memc) {
		php_error_docref(NULL, E_WARNING, "Session is not allocated, check session.save_path value");
		return FAILURE;
	}

	/* Set the number of write retry attempts to the number of replicas times the number of attempts to remove a server plus the initial write */
	if (MEMC_SESS_INI(remove_failed_servers_enabled)) {
		zend_long replicas, failure_limit;

		replicas = memcached_behavior_get(memc, MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS);
		failure_limit = memcached_behavior_get(memc, MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT);

		retries = 1 + replicas * (failure_limit + 1);
	}

	do {
		if (memcached_set(memc, key->val, key->len, val->val, val->len, expiration, 0) == MEMCACHED_SUCCESS) {
			return SUCCESS;
		} else {
			php_error_docref(NULL, E_WARNING, "error saving session to memcached: %s", memcached_last_error_message(memc));
		}
	} while (--retries > 0);

	return FAILURE;
}

PS_DESTROY_FUNC(memcached)
{
	php_memcached_user_data *user_data;
	memcached_st *memc = PS_GET_MOD_DATA();

	if (!memc) {
		php_error_docref(NULL, E_WARNING, "Session is not allocated, check session.save_path value");
		return FAILURE;
	}

	memcached_delete(memc, key->val, key->len, 0);
	user_data = memcached_get_user_data(memc);

	if (user_data->is_locked) {
		s_unlock_session(memc);
	}
	return SUCCESS;
}

PS_GC_FUNC(memcached)
{
	return SUCCESS;
}

PS_CREATE_SID_FUNC(memcached)
{
	zend_string *sid;
	memcached_st *memc = PS_GET_MOD_DATA();

	if (!memc) {
		sid = php_session_create_id(NULL);
	}
	else {
		int retries = 3;
		while (retries-- > 0) {
			sid = php_session_create_id((void **) &memc);

			if (memcached_add (memc, sid->val, sid->len, NULL, 0, s_lock_expiration(), 0) == MEMCACHED_SUCCESS) {
				break;
			}
			zend_string_release(sid);
			sid = NULL;
		}
	}
	return sid;
}

PS_VALIDATE_SID_FUNC(memcached)
{
	memcached_st *memc = PS_GET_MOD_DATA();

	if (php_memcached_exist(memc, key) == MEMCACHED_SUCCESS) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

PS_UPDATE_TIMESTAMP_FUNC(memcached)
{
	memcached_st *memc = PS_GET_MOD_DATA();
	time_t expiration = s_session_expiration(maxlifetime);

	if (php_memcached_touch(memc, key->val, key->len, expiration) == MEMCACHED_FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}
/* }}} */

