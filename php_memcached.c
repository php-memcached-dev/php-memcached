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

/* $ Id: $ */

/* TODO
 * - set LIBKETAMA_COMPATIBLE as the default?
 * - fix unserialize(serialize($memc))
 * - ability to set binary protocol for sessions
 */

#include "php_memcached.h"
#include "php_memcached_private.h"
#include "php_memcached_server.h"
#include "g_fmt.h"

#ifdef HAVE_MEMCACHED_SESSION
# include "php_memcached_session.h"
#endif
#ifdef HAVE_FASTLZ_H
#include <fastlz.h>
#else
#include "fastlz/fastlz.h"
#endif
#include <zlib.h>

#ifdef HAVE_JSON_API
# include "ext/json/php_json.h"
#endif

#ifdef HAVE_MEMCACHED_IGBINARY
# include "ext/igbinary/igbinary.h"
#endif

#ifdef HAVE_MEMCACHED_MSGPACK
# include "ext/msgpack/php_msgpack.h"
#endif


/****************************************
  Custom options
****************************************/
#define MEMC_OPT_COMPRESSION        -1001
#define MEMC_OPT_PREFIX_KEY         -1002
#define MEMC_OPT_SERIALIZER         -1003
#define MEMC_OPT_COMPRESSION_TYPE   -1004
#define MEMC_OPT_STORE_RETRY_COUNT  -1005

/****************************************
  Custom result codes
****************************************/
#define MEMC_RES_PAYLOAD_FAILURE -1001

/****************************************
  Payload value flags
****************************************/
#define MEMC_CREATE_MASK(start, n_bits) (((1 << n_bits) - 1) << start)

#define MEMC_MASK_TYPE     MEMC_CREATE_MASK(0, 4)
#define MEMC_MASK_INTERNAL MEMC_CREATE_MASK(4, 12)
#define MEMC_MASK_USER     MEMC_CREATE_MASK(16, 16)

#define MEMC_VAL_GET_TYPE(flags)       ((flags) & MEMC_MASK_TYPE)
#define MEMC_VAL_SET_TYPE(flags, type) ((flags) |= ((type) & MEMC_MASK_TYPE))

#define MEMC_VAL_IS_STRING     0
#define MEMC_VAL_IS_LONG       1
#define MEMC_VAL_IS_DOUBLE     2
#define MEMC_VAL_IS_BOOL       3
#define MEMC_VAL_IS_SERIALIZED 4
#define MEMC_VAL_IS_IGBINARY   5
#define MEMC_VAL_IS_JSON       6
#define MEMC_VAL_IS_MSGPACK    7

#define MEMC_VAL_COMPRESSED          (1<<0)
#define MEMC_VAL_COMPRESSION_ZLIB    (1<<1)
#define MEMC_VAL_COMPRESSION_FASTLZ  (1<<2)

#define MEMC_VAL_GET_FLAGS(internal_flags)               ((internal_flags & MEMC_MASK_INTERNAL) >> 4)
#define MEMC_VAL_SET_FLAG(internal_flags, internal_flag) ((internal_flags) |= ((internal_flag << 4) & MEMC_MASK_INTERNAL))
#define MEMC_VAL_HAS_FLAG(internal_flags, internal_flag) ((MEMC_VAL_GET_FLAGS(internal_flags) & internal_flag) == internal_flag)
#define MEMC_VAL_DEL_FLAG(internal_flags, internal_flag) internal_flags &= ~((internal_flag << 4) & MEMC_MASK_INTERNAL)

/****************************************
  User-defined flags
****************************************/
#define MEMC_VAL_GET_USER_FLAGS(flags)            ((flags & MEMC_MASK_USER) >> 16)
#define MEMC_VAL_SET_USER_FLAGS(flags, udf_flags) ((flags) |= ((udf_flags << 16) & MEMC_MASK_USER))
#define MEMC_VAL_USER_FLAGS_MAX                   ((1 << 16) - 1)

/****************************************
  "get" operation flags
****************************************/
#define MEMC_GET_PRESERVE_ORDER (1<<0)


/****************************************
  Helper macros
****************************************/
#define MEMC_METHOD_INIT_VARS              \
    zval*             object  = getThis(); \
    php_memc_t*       i_obj   = NULL;      \
    struct memc_obj*  m_obj   = NULL;

#ifndef DVAL_TO_LVAL
#ifdef _WIN64
# define DVAL_TO_LVAL(d, l) \
      if ((d) > LONG_MAX) { \
              (l) = (long)(unsigned long)(__int64) (d); \
      } else { \
              (l) = (long) (d); \
      }
#else
# define DVAL_TO_LVAL(d, l) \
      if ((d) > LONG_MAX) { \
              (l) = (unsigned long) (d); \
      } else { \
              (l) = (long) (d); \
      }
#endif
#endif

#define RETURN_FROM_GET RETURN_FALSE

/****************************************
  Structures and definitions
****************************************/
enum memcached_compression_type {
	COMPRESSION_TYPE_ZLIB = 1,
	COMPRESSION_TYPE_FASTLZ = 2,
};

typedef struct {
	struct memc_obj {
		memcached_st *memc;
		zend_bool compression;
		enum memcached_serializer serializer;
		enum memcached_compression_type compression_type;
#if HAVE_MEMCACHED_SASL
		zend_bool has_sasl_data;
#endif
		long store_retry_count;
	} *obj;

	zend_bool is_persistent;
	zend_bool is_pristine;
	int rescode;
	int memc_errno;
	zend_object zo;
} php_memc_t;

static inline php_memc_t *php_memc_fetch_object(zend_object *obj) {
	return (php_memc_t *)((char *)obj - XtOffsetOf(php_memc_t, zo));
}
#define Z_MEMC_OBJ_P(zv) php_memc_fetch_object(Z_OBJ_P(zv));

#define MEMC_METHOD_FETCH_OBJECT  \
	i_obj = Z_MEMC_OBJ_P(object); \
	m_obj = i_obj->obj; \
	if (!m_obj) {	\
		php_error_docref(NULL, E_WARNING, "Memcached constructor was not called");	\
		return;	\
	}

#ifdef HAVE_MEMCACHED_PROTOCOL

typedef struct {
	zend_object zo;
	php_memc_proto_handler_t *handler;
} php_memc_server_t;

#endif

enum {
	MEMC_OP_SET,
	MEMC_OP_TOUCH,
	MEMC_OP_ADD,
	MEMC_OP_REPLACE,
	MEMC_OP_APPEND,
	MEMC_OP_PREPEND
};

static zend_class_entry *memcached_ce = NULL;

static zend_class_entry *memcached_exception_ce = NULL;

static zend_object_handlers memcached_object_handlers;

#ifdef HAVE_MEMCACHED_PROTOCOL
static zend_object_handlers memcached_server_object_handlers;
static zend_class_entry *memcached_server_ce = NULL;
#endif

struct callbackContext
{
	zval *array;
	zval *entry;
	memcached_stat_st *stats; /* for use with functions that need stats */
	void *return_value;
	unsigned int i; /* for use with structures mapped against servers */
};

#if HAVE_SPL
static zend_class_entry *spl_ce_RuntimeException = NULL;
#endif

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3)
const zend_fcall_info empty_fcall_info = { 0, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0 };
#undef ZEND_BEGIN_ARG_INFO_EX
#define ZEND_BEGIN_ARG_INFO_EX(name, pass_rest_by_reference, return_reference, required_num_args) \
	static zend_arg_info name[] = { \
		{ NULL, 0, NULL, 0, 0, 0, pass_rest_by_reference, return_reference, required_num_args },
#endif

ZEND_DECLARE_MODULE_GLOBALS(php_memcached)

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif

static PHP_INI_MH(OnUpdateCompressionType)
{
	if (!new_value) {
		MEMC_G(compression_type_real) = COMPRESSION_TYPE_FASTLZ;
	} else if (!strcmp(new_value->val, "fastlz")) {
		MEMC_G(compression_type_real) = COMPRESSION_TYPE_FASTLZ;
	} else if (!strcmp(new_value->val, "zlib")) {
		MEMC_G(compression_type_real) = COMPRESSION_TYPE_ZLIB;
	} else {
		return FAILURE;
	}
	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}

static PHP_INI_MH(OnUpdateSerializer)
{
	if (!new_value) {
		MEMC_G(serializer) = SERIALIZER_DEFAULT;
	} else if (!strcmp(new_value->val, "php")) {
		MEMC_G(serializer) = SERIALIZER_PHP;
#ifdef HAVE_MEMCACHED_IGBINARY
	} else if (!strcmp(new_value->val, "igbinary")) {
		MEMC_G(serializer) = SERIALIZER_IGBINARY;
#endif // IGBINARY
#ifdef HAVE_JSON_API
	} else if (!strcmp(new_value->val, "json")) {
		MEMC_G(serializer) = SERIALIZER_JSON;
	} else if (!strcmp(new_value->val, "json_array")) {
		MEMC_G(serializer) = SERIALIZER_JSON_ARRAY;
#endif // JSON
#ifdef HAVE_MEMCACHED_MSGPACK
	} else if (!strcmp(new_value->val, "msgpack")) {
		MEMC_G(serializer) = SERIALIZER_MSGPACK;
#endif // msgpack
	} else {
		return FAILURE;
	}

	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}

/* {{{ INI entries */
PHP_INI_BEGIN()
#ifdef HAVE_MEMCACHED_SESSION
	STD_PHP_INI_ENTRY("memcached.sess_locking",		"1",		PHP_INI_ALL, OnUpdateBool,		sess_locking_enabled,	zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_consistent_hash",	"0",		PHP_INI_ALL, OnUpdateBool,		sess_consistent_hash_enabled,	zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_binary",		"0",		PHP_INI_ALL, OnUpdateBool,		sess_binary_enabled,	zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_lock_wait",		"150000",	PHP_INI_ALL, OnUpdateLongGEZero,sess_lock_wait,			zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_lock_max_wait",		"0",	PHP_INI_ALL, OnUpdateLongGEZero, sess_lock_max_wait,			zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_lock_expire",		"0",	PHP_INI_ALL, OnUpdateLongGEZero, sess_lock_expire,			zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_prefix",		"memc.sess.key.",	PHP_INI_ALL, OnUpdateString, sess_prefix,		zend_php_memcached_globals,	php_memcached_globals)

	STD_PHP_INI_ENTRY("memcached.sess_number_of_replicas",	"0",	PHP_INI_ALL, OnUpdateLongGEZero,	sess_number_of_replicas,	zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_randomize_replica_read",	"0",	PHP_INI_ALL, OnUpdateBool,	sess_randomize_replica_read,	zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_remove_failed",	"0",		PHP_INI_ALL, OnUpdateBool,              sess_remove_failed_enabled,	zend_php_memcached_globals,     php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_connect_timeout",     "1000",         PHP_INI_ALL, OnUpdateLong, 		sess_connect_timeout,           zend_php_memcached_globals,     php_memcached_globals)
#if HAVE_MEMCACHED_SASL
	STD_PHP_INI_ENTRY("memcached.sess_sasl_username",		"",	PHP_INI_ALL, OnUpdateString, sess_sasl_username,		zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_sasl_password",		"",	PHP_INI_ALL, OnUpdateString, sess_sasl_password,		zend_php_memcached_globals,	php_memcached_globals)
#endif
#endif
	STD_PHP_INI_ENTRY("memcached.compression_type",		"fastlz",	PHP_INI_ALL, OnUpdateCompressionType, compression_type,		zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.compression_factor",	"1.3",		PHP_INI_ALL, OnUpdateReal, compression_factor,		zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.compression_threshold",	"2000",		PHP_INI_ALL, OnUpdateLong, compression_threshold,		zend_php_memcached_globals,	php_memcached_globals)

	STD_PHP_INI_ENTRY("memcached.serializer",		SERIALIZER_DEFAULT_NAME, PHP_INI_ALL, OnUpdateSerializer, serializer_name,	zend_php_memcached_globals,	php_memcached_globals)
#if HAVE_MEMCACHED_SASL
	STD_PHP_INI_ENTRY("memcached.use_sasl",	"0", PHP_INI_SYSTEM, OnUpdateBool, use_sasl,	zend_php_memcached_globals,	php_memcached_globals)
#endif
	STD_PHP_INI_ENTRY("memcached.store_retry_count",	"2",		PHP_INI_ALL, OnUpdateLong, store_retry_count,			zend_php_memcached_globals,     php_memcached_globals)
PHP_INI_END()
/* }}} */

/****************************************
  Forward declarations
****************************************/
static int php_memc_handle_error(php_memc_t *i_obj, memcached_return status);
static char *php_memc_zval_to_payload(zval *value, size_t *payload_len, uint32_t *flags, enum memcached_serializer serializer, enum memcached_compression_type compression_type);
static int php_memc_zval_from_payload(zval *value, const char *payload, size_t payload_len, uint32_t flags, enum memcached_serializer serializer);
static void php_memc_get_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_store_impl(INTERNAL_FUNCTION_PARAMETERS, int op, zend_bool by_key);
static void php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_delete_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static memcached_return php_memc_do_cache_callback(zval *memc_obj, zend_fcall_info *fci, zend_fcall_info_cache *fcc, zend_string *key, zval *value);
static int php_memc_do_result_callback(zval *memc_obj, zend_fcall_info *fci, zend_fcall_info_cache *fcc, memcached_result_st *result);
static memcached_return php_memc_do_serverlist_callback(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context);
static memcached_return php_memc_do_stats_callback(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context);
static memcached_return php_memc_do_version_callback(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context);
static void php_memc_destroy(struct memc_obj *m_obj, zend_bool persistent);

/****************************************
  Method implementations
****************************************/


char *php_memc_printable_func (zend_fcall_info *fci, zend_fcall_info_cache *fci_cache)
{
	char *buffer = NULL;

	if (fci->object) {
		spprintf (&buffer, 0, "%s::%s", fci->object->ce->name->val, fci_cache->function_handler->common.function_name);
	} else {
		if (Z_TYPE (fci->function_name) == IS_OBJECT) {
			spprintf (&buffer, 0, "%s", Z_OBJCE (fci->function_name)->name->val);
		}
		else {
			spprintf (&buffer, 0, "%s", Z_STRVAL (fci->function_name));
		}
	}
	return buffer;
}

static zend_bool php_memcached_on_new_callback(zval *object, zend_fcall_info *fci, zend_fcall_info_cache *fci_cache, zend_string *persistent_id)
{
	zend_bool ret = 1;
	zval retval, id;

	if (persistent_id) {
		ZVAL_STR(&id, persistent_id);
	} else {
		ZVAL_NULL(&id);
	}

	ZVAL_UNDEF(&retval);

	zend_fcall_info_argn(fci, 2, object, &id);
	fci->retval         = &retval;
	fci->no_separation  = 1;

	if (zend_call_function(fci, fci_cache) == FAILURE) {
		char *buf = php_memc_printable_func (fci, fci_cache);
		php_error_docref(NULL, E_WARNING, "Failed to invoke 'on_new' callback %s()", buf);
		efree (buf);
		ret = 0;
	}
	
	if (Z_TYPE(retval) != IS_UNDEF)
		zval_ptr_dtor(&retval);

	zend_fcall_info_args_clear(fci, 1);

	return ret;
}

static int le_memc, le_memc_sess;

static int php_memc_list_entry(void)
{
	return le_memc;
}

/* {{{ Memcached::__construct([string persistent_id[, callback on_new[, string connection_str]]]))
   Creates a Memcached object, optionally using persistent memcache connection */
static PHP_METHOD(Memcached, __construct)
{
	zval *object = getThis();
	php_memc_t *i_obj;
	struct memc_obj *m_obj = NULL;
	zend_string *persistent_id = NULL;
	zend_string *conn_str = NULL;
	zend_bool is_persistent = 0;
	zend_string *plist_key = NULL;
	zend_fcall_info fci = {0};
	zend_fcall_info_cache fci_cache;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S!f!S", &persistent_id, &fci, &fci_cache, &conn_str) == FAILURE) {
		return;
	}

	i_obj = Z_MEMC_OBJ_P(object);
	i_obj->is_pristine = 0;

	if (persistent_id && persistent_id->len) {
		zend_resource *le;

		is_persistent = 1;
		plist_key = zend_string_alloc(sizeof("memcached:id=") + persistent_id->len - 1, 0);
		snprintf(plist_key->val, plist_key->len + 1, "memcached:id=%s", persistent_id->val);
		if ((le = zend_hash_find_ptr(&EG(persistent_list), plist_key)) != NULL) {
			if (le->type == php_memc_list_entry()) {
				m_obj = (struct memc_obj *) le->ptr;
			}
		}
		i_obj->obj = m_obj;
	}

	i_obj->is_persistent = is_persistent;

	if (!m_obj) {
		m_obj = pecalloc(1, sizeof(*m_obj), is_persistent);
		if (m_obj == NULL) {
			if (plist_key) {
				zend_string_release(plist_key);
			}
			php_error_docref(NULL, E_ERROR, "out of memory: cannot allocate handle");
			/* not reached */
		}

		if (conn_str) {
			m_obj->memc = php_memc_create_str(conn_str->val, conn_str->len);
			if (!m_obj->memc) {
				char error_buffer[1024];
				if (plist_key) {
					zend_string_release(plist_key);
				}
#ifdef HAVE_LIBMEMCACHED_CHECK_CONFIGURATION
				if (libmemcached_check_configuration(conn_str->val, conn_str->len, error_buffer, sizeof(error_buffer)) != MEMCACHED_SUCCESS) {
					php_error_docref(NULL, E_ERROR, "configuration error %s", error_buffer);
				} else {
					php_error_docref(NULL, E_ERROR, "could not allocate libmemcached structure");
				}
#else
				php_error_docref(NULL, E_ERROR, "could not allocate libmemcached structure");
#endif
				/* not reached */
			}
		} else {
			m_obj->memc = memcached_create(NULL);
			if (m_obj->memc == NULL) {
				if (plist_key) {
					zend_string_release(plist_key);
				}
				php_error_docref(NULL, E_ERROR, "could not allocate libmemcached structure");
				/* not reached */
			}
		}

		m_obj->serializer = MEMC_G(serializer);
		m_obj->compression_type = MEMC_G(compression_type_real);
		m_obj->compression = 1;
		m_obj->store_retry_count = MEMC_G(store_retry_count);

		i_obj->obj = m_obj;
		i_obj->is_pristine = 1;

		if (fci.size) { /* will be 0 when not available */
			if (!php_memcached_on_new_callback(object, &fci, &fci_cache, persistent_id) || EG(exception)) {
				/* error calling or exception thrown from callback */
				if (plist_key) {
					zend_string_release(plist_key);
				}

				i_obj->obj = NULL;
				if (is_persistent) {
					php_memc_destroy(m_obj, is_persistent);
				}

				return;
			}
		}

		if (is_persistent) {
			zend_resource le;

			le.type = php_memc_list_entry();
			le.ptr = m_obj;
			GC_REFCOUNT(&le) = 1;
			/* plist_key is not a persistent allocated key, thus we use str_update here */
			if (zend_hash_str_update_mem(&EG(persistent_list), plist_key->val, plist_key->len, &le, sizeof(le)) == NULL) {
				if (plist_key) {
					zend_string_release(plist_key);
				}
				php_error_docref(NULL, E_ERROR, "could not register persistent entry");
				/* not reached */
			}
		}
	}

	if (plist_key) {
		zend_string_release(plist_key);
	}
}
/* }}} */

/* {{{ Memcached::get(string key [, mixed callback [, double &cas_token [, int &udf_flags ] ] ])
   Returns a value for the given key or false */
PHP_METHOD(Memcached, get)
{
	php_memc_get_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getByKey(string server_key, string key [, mixed callback [, double &cas_token [, int &udf_flags ] ] ])
   Returns a value for key from the server identified by the server key or false */
PHP_METHOD(Memcached, getByKey)
{
	php_memc_get_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_get_impl */
static void php_memc_get_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zend_string *key;
	zend_string *server_key = NULL;
	const char  *payload = NULL;
	size_t payload_len = 0;
	uint32_t flags = 0;
	uint64_t cas = 0;
	const char* keys[1] = { NULL };
	size_t key_lens[1] = { 0 };
	zval *cas_token = NULL;
	zval *udf_flags = NULL;
	uint64_t orig_cas_flag;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|f!zz", &server_key, &key,
								  &fci, &fcc, &cas_token, &udf_flags) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|f!zz", &key,
								  &fci, &fcc, &cas_token, &udf_flags) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key->len == 0 || strchr(key->val, ' ')) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FROM_GET;
	}

	keys[0] = key->val;
	key_lens[0] = key->len;

	orig_cas_flag = memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS);

	/*
		* Enable CAS support, but only if it is currently disabled.
		*/
	if (cas_token && Z_ISREF_P(cas_token) && orig_cas_flag == 0) {
		memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);
	}

	if (by_key) {
		status = memcached_mget_by_key(m_obj->memc, server_key->val, server_key->len, keys, key_lens, 1);
	} else {
		status = memcached_mget(m_obj->memc, keys, key_lens, 1);
	}
	if (cas_token && Z_ISREF_P(cas_token) && orig_cas_flag == 0) {
		memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
	}

	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FROM_GET;
	}

	status = MEMCACHED_SUCCESS;
	memcached_result_create(m_obj->memc, &result);

	if (memcached_fetch_result(m_obj->memc, &result, &status) == NULL) {
		/* This is for historical reasons */
		if (status == MEMCACHED_END)
			status = MEMCACHED_NOTFOUND;

		/*
			* If the result wasn't found, and we have the read-through callback, invoke
			* it to get the value. The CAS token will be 0, because we cannot generate it
			* ourselves.
			*/
		if (cas_token) {
			ZVAL_DEREF(cas_token);
			zval_ptr_dtor(cas_token);
			ZVAL_DOUBLE(cas_token, 0.0);
		}

		if (status == MEMCACHED_NOTFOUND && fci.size != 0) {
			status = php_memc_do_cache_callback(getThis(), &fci, &fcc, key, return_value);
		}

		if (php_memc_handle_error(i_obj, status) < 0) {
			memcached_result_free(&result);
			RETURN_FROM_GET;
		}

		/* if we have a callback, all processing is done */
		if (fci.size != 0) {
			memcached_result_free(&result);
			return;
		}
	}

	/* Fetch all remaining results */
	memcached_result_st dummy_result;
	memcached_return dummy_status = MEMCACHED_SUCCESS;
	memcached_result_create(m_obj->memc, &dummy_result);
	while (memcached_fetch_result(m_obj->memc, &dummy_result, &dummy_status) != NULL) {}
	memcached_result_free(&dummy_result);

	payload     = memcached_result_value(&result);
	payload_len = memcached_result_length(&result);
	flags       = memcached_result_flags(&result);
	if (cas_token) {
		cas     = memcached_result_cas(&result);
	}

	if (php_memc_zval_from_payload(return_value, payload, payload_len, flags, m_obj->serializer) < 0) {
		memcached_result_free(&result);
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FROM_GET;
	}

	if (cas_token) {
		ZVAL_DEREF(cas_token);
		zval_ptr_dtor(cas_token);
		ZVAL_DOUBLE(cas_token, (double)cas);
	}

	if (udf_flags) {
		ZVAL_DEREF(udf_flags);
		SEPARATE_ZVAL(udf_flags);
		zval_dtor(udf_flags);
		ZVAL_LONG(udf_flags, MEMC_VAL_GET_USER_FLAGS(flags));
	}

	memcached_result_free(&result);
}
/* }}} */

/* {{{ Memcached::getMulti(array keys [, array &cas_tokens [, array &udf_flags ] ])
   Returns values for the given keys or false */
PHP_METHOD(Memcached, getMulti)
{
	php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getMultiByKey(string server_key, array keys [, array &cas_tokens [, array &udf_flags ] ])
   Returns values for the given keys from the server identified by the server key or false */
PHP_METHOD(Memcached, getMultiByKey)
{
	php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_getMulti_impl */
static void php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *keys = NULL;
	zend_string *server_key = NULL;
	size_t num_keys = 0;
	zval *entry = NULL;
	const char  *payload = NULL;
	size_t payload_len = 0;
	const char **mkeys = NULL;
	size_t *mkeys_len = NULL;
	const char *tmp_key = NULL;
	size_t res_key_len = 0;
	uint32_t flags;
	uint64_t cas = 0;
	zval *cas_tokens = NULL;
	zval *udf_flags = NULL;
	uint64_t orig_cas_flag = 0;
	zval value;
	long get_flags = 0;
	int i = 0;
	zend_bool preserve_order;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa/|zlz", &server_key,
								  &keys, &cas_tokens, &get_flags, &udf_flags) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "a/|zlz", &keys, &cas_tokens, &get_flags, &udf_flags) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	preserve_order = (get_flags & MEMC_GET_PRESERVE_ORDER);
	num_keys  = zend_hash_num_elements(Z_ARRVAL_P(keys));
	mkeys     = safe_emalloc(num_keys, sizeof(*mkeys), 0);
	mkeys_len = safe_emalloc(num_keys, sizeof(*mkeys_len), 0);
	array_init(return_value);

	/*
	 * Create the array of keys for libmemcached. If none of the keys were valid
	 * (strings), set bad key result code and return.
	 */
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(keys), entry) {
		if (Z_TYPE_P(entry) != IS_STRING) {
			convert_to_string_ex(entry);
		}

		if (Z_TYPE_P(entry) == IS_STRING && Z_STRLEN_P(entry) > 0) {
			mkeys[i]     = Z_STRVAL_P(entry);
			mkeys_len[i] = Z_STRLEN_P(entry);

			if (preserve_order) {
				add_assoc_null_ex(return_value, mkeys[i], mkeys_len[i]);
			}
			i++;
		}
	} ZEND_HASH_FOREACH_END();

	if (i == 0) {
		i_obj->rescode = MEMCACHED_NOTFOUND;
		efree(mkeys);
		efree(mkeys_len);
		return;
	}

	/*
	 * Enable CAS support, but only if it is currently disabled.
	 */
	if (cas_tokens && Z_ISREF_P(cas_tokens)) {
		orig_cas_flag = memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS);
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);
		}
	}

	if (by_key) {
		status = memcached_mget_by_key(m_obj->memc, server_key->val, server_key->len, mkeys, mkeys_len, i);
	} else {
		status = memcached_mget(m_obj->memc, mkeys, mkeys_len, i);
	}
	/* Handle error, but ignore, there might still be some result */
	php_memc_handle_error(i_obj, status);

	/*
	 * Restore the CAS support flag, but only if we had to turn it on.
	 */
	if (cas_tokens && Z_ISREF_P(cas_tokens) && orig_cas_flag == 0) {
		memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
	}

	efree(mkeys);
	efree(mkeys_len);

	/*
	 * Iterate through the result set and create the result array. The CAS tokens are
	 * returned as doubles, because we cannot store potential 64-bit values in longs.
	 */
	if (cas_tokens) {
		if (Z_ISREF_P(cas_tokens)) {
			/* cas_tokens was passed by reference, we'll create an array for it. */
			ZVAL_DEREF(cas_tokens);
			SEPARATE_ZVAL(cas_tokens);
			zval_dtor(cas_tokens);
			array_init(cas_tokens);
		} else {
			/* Not passed by reference, we allow this (eg.: if you specify null
			 to not enable cas but you want to use the udf_flags parameter).
			 We destruct it and set it to null for the peace of mind. */
			cas_tokens = NULL;
		}
	}

	/*
	 * Iterate through the result set and create the result array. The flags are
	 * returned as longs.
	 */
	if (udf_flags) {
		ZVAL_DEREF(udf_flags);
		SEPARATE_ZVAL(udf_flags);
		zval_dtor(udf_flags);
		array_init(udf_flags);
	}

	memcached_result_create(m_obj->memc, &result);
	while ((memcached_fetch_result(m_obj->memc, &result, &status)) != NULL) {
		char res_key [MEMCACHED_MAX_KEY];

		if (status != MEMCACHED_SUCCESS) {
			status = MEMCACHED_SOME_ERRORS;
			php_memc_handle_error(i_obj, status);
			continue;
		}

		payload     = memcached_result_value(&result);
		payload_len = memcached_result_length(&result);
		flags       = memcached_result_flags(&result);
		tmp_key     = memcached_result_key_value(&result);
		res_key_len = memcached_result_key_length(&result);

		/*
		 * This may be a bug in libmemcached, the key is not null terminated
		 * whe using the binary protocol.
		 */
		memcpy (res_key, tmp_key, res_key_len >= MEMCACHED_MAX_KEY ? MEMCACHED_MAX_KEY - 1 : res_key_len);
		res_key [res_key_len] = '\0';

		if (php_memc_zval_from_payload(&value, payload, payload_len, flags, m_obj->serializer) < 0) {
			zval_ptr_dtor(&value);
			if (EG(exception)) {
				status = MEMC_RES_PAYLOAD_FAILURE;
				php_memc_handle_error(i_obj, status);
				memcached_quit(m_obj->memc);

				break;
			}
			status = MEMCACHED_SOME_ERRORS;
			i_obj->rescode = MEMCACHED_SOME_ERRORS;

			continue;
		}

		add_assoc_zval_ex(return_value, res_key, res_key_len, &value);
		if (cas_tokens) {
			cas = memcached_result_cas(&result);
			add_assoc_double_ex(cas_tokens, res_key, res_key_len, (double)cas);
		}
		if (udf_flags) {
			add_assoc_long_ex(udf_flags, res_key, res_key_len, MEMC_VAL_GET_USER_FLAGS(flags));
		}
	}

	memcached_result_free(&result);

	if (EG(exception)) {
		/* XXX: cas_tokens should only be set on success, currently we're destructive */
		if (cas_tokens) {
			ZVAL_DEREF(cas_tokens);
			SEPARATE_ZVAL(cas_tokens);
			zval_dtor(cas_tokens);
			ZVAL_NULL(cas_tokens);
		}
		if (udf_flags) {
			ZVAL_DEREF(udf_flags);
			SEPARATE_ZVAL(udf_flags);
			zval_dtor(udf_flags);
			ZVAL_NULL(udf_flags);
		}
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::getDelayed(array keys [, bool with_cas [, mixed callback ] ])
   Sends a request for the given keys and returns immediately */
PHP_METHOD(Memcached, getDelayed)
{
	php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getDelayedByKey(string server_key, array keys [, bool with_cas [, mixed callback ] ])
   Sends a request for the given keys from the server identified by the server key and returns immediately */
PHP_METHOD(Memcached, getDelayedByKey)
{
	php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_getDelayed_impl */
static void php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *keys = NULL;
	zend_string *server_key = NULL;
	zend_bool with_cas = 0;
	size_t num_keys = 0;
	zval *entry = NULL;
	const char **mkeys = NULL;
	size_t *mkeys_len = NULL;
	uint64_t orig_cas_flag = 0;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	int i = 0;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa/|bf!", &server_key,
								  &keys, &with_cas, &fci, &fcc) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "a/|bf!", &keys, &with_cas,
								  &fci, &fcc) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	/*
	 * Create the array of keys for libmemcached. If none of the keys were valid
	 * (strings), set bad key result code and return.
	 */
	num_keys  = zend_hash_num_elements(Z_ARRVAL_P(keys));
	mkeys     = safe_emalloc(num_keys, sizeof(*mkeys), 0);
	mkeys_len = safe_emalloc(num_keys, sizeof(*mkeys_len), 0);

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(keys), entry) {
		if (Z_TYPE_P(entry) != IS_STRING) {
			convert_to_string_ex(entry);
		}

		if (Z_TYPE_P(entry) == IS_STRING && Z_STRLEN_P(entry) > 0) {
			mkeys[i]     = Z_STRVAL_P(entry);
			mkeys_len[i] = Z_STRLEN_P(entry);
			i++;
		}
	} ZEND_HASH_FOREACH_END();

	if (i == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		efree(mkeys);
		efree(mkeys_len);
		zval_dtor(return_value);
		RETURN_FALSE;
	}

	/*
	 * Enable CAS support, but only if it is currently disabled.
	 */
	if (with_cas) {
		orig_cas_flag = memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS);
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);
		}
	}

	/*
	 * Issue the request, but collect results only if the result callback is provided.
	 */
	if (by_key) {
		status = memcached_mget_by_key(m_obj->memc, server_key->val, server_key->len, mkeys, mkeys_len, i);
	} else {
		status = memcached_mget(m_obj->memc, mkeys, mkeys_len, i);
	}
	/*
	 * Restore the CAS support flag, but only if we had to turn it on.
	 */
	if (with_cas && orig_cas_flag == 0) {
		memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
	}

	efree(mkeys);
	efree(mkeys_len);
	if (php_memc_handle_error(i_obj, status) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}

	if (fci.size != 0) {
		/*
		 * We have a result callback. Iterate through the result set and invoke the
		 * callback for each one.
		 */
		memcached_result_st result;

		memcached_result_create(m_obj->memc, &result);
		while ((memcached_fetch_result(m_obj->memc, &result, &status)) != NULL) {
			if (php_memc_do_result_callback(getThis(), &fci, &fcc, &result) < 0) {
				status = MEMCACHED_FAILURE;
				break;
			}
		}
		memcached_result_free(&result);

		/* we successfully retrieved all rows */
		if (status == MEMCACHED_END) {
			status = MEMCACHED_SUCCESS;
		}
		if (php_memc_handle_error(i_obj, status) < 0) {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::fetch()
   Returns the next result from a previous delayed request */
PHP_METHOD(Memcached, fetch)
{
	const char  *res_key = NULL;
	size_t res_key_len = 0;
	const char  *payload = NULL;
	size_t payload_len = 0;
	zval  value;
	uint32_t flags = 0;
	uint64_t cas = 0;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	memcached_result_create(m_obj->memc, &result);
	if ((memcached_fetch_result(m_obj->memc, &result, &status)) == NULL) {
		php_memc_handle_error(i_obj, status);
		memcached_result_free(&result);
		RETURN_FALSE;
	}

	payload     = memcached_result_value(&result);
	payload_len = memcached_result_length(&result);
	flags       = memcached_result_flags(&result);
	res_key     = memcached_result_key_value(&result);
	res_key_len = memcached_result_key_length(&result);
	cas         = memcached_result_cas(&result);

	if (php_memc_zval_from_payload(&value, payload, payload_len, flags, m_obj->serializer) < 0) {
		memcached_result_free(&result);
		zval_ptr_dtor(&value);
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_stringl_ex(return_value, ZEND_STRL("key"), (char *)res_key, res_key_len);
	add_assoc_zval_ex(return_value, ZEND_STRL("value"), &value);
	if (cas != 0) {
		/* XXX: also check against ULLONG_MAX or memc_behavior */
		add_assoc_double_ex(return_value, ZEND_STRL("cas"), (double)cas);
	}
	if (MEMC_VAL_GET_USER_FLAGS(flags) != 0) {
		add_assoc_long_ex(return_value, ZEND_STRL("flags"), MEMC_VAL_GET_USER_FLAGS(flags));
	}

	memcached_result_free(&result);
}
/* }}} */

/* {{{ Memcached::fetchAll()
   Returns all the results from a previous delayed request */
PHP_METHOD(Memcached, fetchAll)
{
	const char  *res_key = NULL;
	size_t res_key_len = 0;
	const char  *payload = NULL;
	size_t payload_len = 0;
	zval  value, entry;
	uint32_t flags;
	uint64_t cas = 0;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	array_init(return_value);
	memcached_result_create(m_obj->memc, &result);

	while ((memcached_fetch_result(m_obj->memc, &result, &status)) != NULL) {
		payload     = memcached_result_value(&result);
		payload_len = memcached_result_length(&result);
		flags       = memcached_result_flags(&result);
		res_key     = memcached_result_key_value(&result);
		res_key_len = memcached_result_key_length(&result);
		cas         = memcached_result_cas(&result);

		if (php_memc_zval_from_payload(&value, payload, payload_len, flags, m_obj->serializer) < 0) {
			memcached_result_free(&result);
			zval_ptr_dtor(&value);
			zval_dtor(return_value);
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			RETURN_FALSE;
		}

		array_init(&entry);
		add_assoc_stringl_ex(&entry, ZEND_STRL("key"), (char *)res_key, res_key_len);
		add_assoc_zval_ex(&entry, ZEND_STRL("value"), &value);
		if (cas != 0) {
			/* XXX: also check against ULLONG_MAX or memc_behavior */
			add_assoc_double_ex(&entry, ZEND_STRL("cas"), (double)cas);
		}
		if (MEMC_VAL_GET_USER_FLAGS(flags) != 0) {
			add_assoc_long_ex(&entry, ZEND_STRL("flags"), MEMC_VAL_GET_USER_FLAGS(flags));
		}
		add_next_index_zval(return_value, &entry);
	}

	memcached_result_free(&result);

	if (status != MEMCACHED_END && php_memc_handle_error(i_obj, status) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::set(string key, mixed value [, int expiration [, int udf_flags ] ])
   Sets the value for the given key */
PHP_METHOD(Memcached, set)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_SET, 0);
}
/* }}} */

/* {{{ Memcached::setByKey(string server_key, string key, mixed value [, int expiration [, int udf_flags ] ])
   Sets the value for the given key on the server identified by the server key */
PHP_METHOD(Memcached, setByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_SET, 1);
}
/* }}} */

#ifdef HAVE_MEMCACHED_TOUCH
/* {{{ Memcached::touch(string key, [, int expiration ])
   Sets a new expiration for the given key */
PHP_METHOD(Memcached, touch)
{
    php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_TOUCH, 0);
}
/* }}} */

/* {{{ Memcached::touchbyKey(string key, [, int expiration ])
   Sets a new expiration for the given key */
PHP_METHOD(Memcached, touchByKey)
{
    php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_TOUCH, 1);
}
/* }}} */
#endif


/* {{{ Memcached::setMulti(array items [, int expiration [, int udf_flags ] ])
   Sets the keys/values specified in the items array */
PHP_METHOD(Memcached, setMulti)
{
	php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::setMultiByKey(string server_key, array items [, int expiration [, int udf_flags ] ])
   Sets the keys/values specified in the items array on the server identified by the given server key */
PHP_METHOD(Memcached, setMultiByKey)
{
	php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

#define PHP_MEMC_FAILOVER_RETRY	\
	if (!by_key && retry < m_obj->store_retry_count) {	\
		switch (i_obj->rescode) {	\
			case MEMCACHED_HOST_LOOKUP_FAILURE:	\
			case MEMCACHED_CONNECTION_FAILURE:	\
			case MEMCACHED_CONNECTION_BIND_FAILURE:	\
			case MEMCACHED_WRITE_FAILURE:	\
			case MEMCACHED_READ_FAILURE:	\
			case MEMCACHED_UNKNOWN_READ_FAILURE:	\
			case MEMCACHED_PROTOCOL_ERROR:	\
			case MEMCACHED_SERVER_ERROR:	\
			case MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE:	\
			case MEMCACHED_TIMEOUT:	\
			case MEMCACHED_FAIL_UNIX_SOCKET:	\
			case MEMCACHED_SERVER_MARKED_DEAD:	\
			case MEMCACHED_SERVER_TEMPORARILY_DISABLED:	\
				if (memcached_server_count(m_obj->memc) > 0) {	\
					retry++;	\
					i_obj->rescode = 0;	\
					goto retry;	\
				}	\
				break;	\
		}	\
	}

/* {{{ -- php_memc_setMulti_impl */
static void php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *entries;
	zend_string *server_key = NULL;
	time_t expiration = 0;
	long udf_flags = 0;
	zval *entry;
	zend_string *skey, *str_key = NULL;
	ulong num_key;
	char  *payload;
	size_t payload_len;
	uint32_t flags = 0;
	uint32_t retry = 0;
	memcached_return status;
	char  tmp_key[MEMCACHED_MAX_KEY];
	int tmp_len = 0;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa|ll", &server_key,
								  &entries, &expiration, &udf_flags) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|ll", &entries, &expiration, &udf_flags) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	/*
	 * php_memcached uses 16 bits internally to store type, compression and serialization info.
	 * We use 16 upper bits to store user defined flags.
	 */
	if (udf_flags > 0) {
		if ((uint32_t) udf_flags > MEMC_VAL_USER_FLAGS_MAX) {
			php_error_docref(NULL, E_WARNING, "udf_flags will be limited to %u", MEMC_VAL_USER_FLAGS_MAX);
		}
	}

	ZEND_HASH_FOREACH_KEY_VAL (Z_ARRVAL_P(entries), num_key, skey, entry) {
		if (skey) {
			str_key = skey;
		} else if (num_key || num_key == 0) {
			/* Array keys are unsigned, but php integers are signed.
			 * Keys must be converted to signed strings that match
			 * php integers. */
			assert(sizeof(tmp_key) >= sizeof(ZEND_TOSTR(LONG_MIN)));
			tmp_len = sprintf(tmp_key, "%ld", (long)num_key);
			str_key = zend_string_init(tmp_key, tmp_len, 0);
		} else {
			continue;
		}

		flags = 0;
		if (m_obj->compression) {
			MEMC_VAL_SET_FLAG(flags, MEMC_VAL_COMPRESSED);
		}

		if (udf_flags > 0) {
			MEMC_VAL_SET_USER_FLAGS(flags, ((uint32_t) udf_flags));
		}

		payload = php_memc_zval_to_payload(entry, &payload_len, &flags, m_obj->serializer, m_obj->compression_type);
		if (payload == NULL) {
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			if (!skey) {
				zend_string_release(str_key);
			}
			RETURN_FALSE;
		}

retry:
		if (!by_key) {
			status = memcached_set(m_obj->memc, str_key->val, str_key->len, payload, payload_len, expiration, flags);
		} else {
			status = memcached_set_by_key(m_obj->memc, server_key->val, server_key->len, str_key->val, str_key->len, payload, payload_len, expiration, flags);
		}

		if (php_memc_handle_error(i_obj, status) < 0) {
			PHP_MEMC_FAILOVER_RETRY
			if (!skey) {
				zend_string_release(str_key);
			}
			efree(payload);
			RETURN_FALSE;
		}
		if (!skey) {
			zend_string_release(str_key);
		}
		efree(payload);
	} ZEND_HASH_FOREACH_END();

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::add(string key, mixed value [, int expiration [, int udf_flags ] ])
   Sets the value for the given key, failing if the key already exists */
PHP_METHOD(Memcached, add)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_ADD, 0);
}
/* }}} */

/* {{{ Memcached::addByKey(string server_key, string key, mixed value [, int expiration [, int udf_flags ] ])
   Sets the value for the given key on the server identified by the sever key, failing if the key already exists */
PHP_METHOD(Memcached, addByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_ADD, 1);
}
/* }}} */

/* {{{ Memcached::append(string key, mixed value)
   Appends the value to existing one for the key */
PHP_METHOD(Memcached, append)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_APPEND, 0);
}
/* }}} */

/* {{{ Memcached::appendByKey(string server_key, string key, mixed value)
   Appends the value to existing one for the key on the server identified by the server key */
PHP_METHOD(Memcached, appendByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_APPEND, 1);
}
/* }}} */

/* {{{ Memcached::prepend(string key, mixed value)
   Prepends the value to existing one for the key */
PHP_METHOD(Memcached, prepend)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_PREPEND, 0);
}
/* }}} */

/* {{{ Memcached::prependByKey(string server_key, string key, mixed value)
   Prepends the value to existing one for the key on the server identified by the server key */
PHP_METHOD(Memcached, prependByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_PREPEND, 1);
}
/* }}} */

/* {{{ Memcached::replace(string key, mixed value [, int expiration [, int udf_flags ] ])
   Replaces the value for the given key, failing if the key doesn't exist */
PHP_METHOD(Memcached, replace)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_REPLACE, 0);
}
/* }}} */

/* {{{ Memcached::replaceByKey(string server_key, string key, mixed value [, int expiration [, int udf_flags ] ])
   Replaces the value for the given key on the server identified by the server key, failing if the key doesn't exist */
PHP_METHOD(Memcached, replaceByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_REPLACE, 1);
}
/* }}} */

/* {{{ -- php_memc_store_impl */
static void php_memc_store_impl(INTERNAL_FUNCTION_PARAMETERS, int op, zend_bool by_key)
{
	zend_string *key;
	zend_string *server_key = NULL;
	zend_string *s_value;
	zval  s_zvalue;
	zval *value;
	long expiration = 0;
	long udf_flags = 0;
	char  *payload = NULL;
	size_t payload_len;
	uint32_t flags = 0;
	uint32_t retry = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "SSS", &server_key, &key, &s_value) == FAILURE) {
				return;
			}
			value = &s_zvalue;
			ZVAL_STR(value, s_value);
		} else if (op == MEMC_OP_TOUCH) {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|l", &server_key, &key, &expiration) == FAILURE) {
				return;
			}
		} else {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "SSz|ll", &server_key, &key, &value, &expiration, &udf_flags) == FAILURE) {
				return;
			}
		}
	} else {
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &key, &s_value) == FAILURE) {
				return;
			}
			value = &s_zvalue;
			ZVAL_STR(value, s_value);
		} else if (op == MEMC_OP_TOUCH) {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &key, &expiration) == FAILURE) {
				return;
			}
		} else {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz|ll", &key, &value, &expiration, &udf_flags) == FAILURE) {
				return;
			}
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key->len == 0 || strchr(key->val, ' ')) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	if (m_obj->compression) {
		/*
		 * When compression is enabled, we cannot do appends/prepends because that would
		 * corrupt the compressed values. It is up to the user to fetch the value,
		 * append/prepend new data, and store it again.
		 */
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			php_error_docref(NULL, E_WARNING, "cannot append/prepend with compression turned on");
			return;
		}
		MEMC_VAL_SET_FLAG(flags, MEMC_VAL_COMPRESSED);
	}

	/*
	 * php_memcached uses 16 bits internally to store type, compression and serialization info.
	 * We use 16 upper bits to store user defined flags.
	 */
	if (udf_flags > 0) {
		if ((uint32_t) udf_flags > MEMC_VAL_USER_FLAGS_MAX) {
			php_error_docref(NULL, E_WARNING, "udf_flags will be limited to %u", MEMC_VAL_USER_FLAGS_MAX);
		}
		MEMC_VAL_SET_USER_FLAGS(flags, ((uint32_t) udf_flags));
	}

	if (op == MEMC_OP_TOUCH) {
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX < 0x01000016
		if (memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL)) {
			php_error_docref(NULL, E_WARNING, "using touch command with binary protocol is not recommended with libmemcached versions below 1.0.16");
		}
#endif
	} else {
		payload = php_memc_zval_to_payload(value, &payload_len, &flags, m_obj->serializer, m_obj->compression_type);
		if (payload == NULL) {
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			RETURN_FALSE;
		}
	}
retry:
	switch (op) {
		case MEMC_OP_SET:
			if (!server_key) {
				status = memcached_set(m_obj->memc, key->val, key->len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_set_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
										  key->len, payload, payload_len, expiration, flags);
			}
			break;
#ifdef HAVE_MEMCACHED_TOUCH
		case MEMC_OP_TOUCH:
			if (!server_key) {
				status = memcached_touch(m_obj->memc, key->val, key->len, expiration);
			} else {
				status = memcached_touch_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
										  key->len, expiration);
			}
			break;
#endif
		case MEMC_OP_ADD:
			if (!server_key) {
				status = memcached_add(m_obj->memc, key->val, key->len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_add_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
										  key->len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_REPLACE:
			if (!server_key) {
				status = memcached_replace(m_obj->memc, key->val, key->len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_replace_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
										      key->len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_APPEND:
			if (!server_key) {
				status = memcached_append(m_obj->memc, key->val, key->len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_append_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
											 key->len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_PREPEND:
			if (!server_key) {
				status = memcached_prepend(m_obj->memc, key->val, key->len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_prepend_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
											  key->len, payload, payload_len, expiration, flags);
			}
			break;

		default:
			/* not reached */
			status = 0;
			assert(0);
			break;
	}

	if (php_memc_handle_error(i_obj, status) < 0) {
		PHP_MEMC_FAILOVER_RETRY
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	if (payload) {
		efree(payload);
	}
}
/* }}} */

/* {{{ -- php_memc_cas_impl */
static void php_memc_cas_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	double cas_d;
	uint64_t cas;
	zend_string *key;
	zend_string *server_key = NULL;
	zval *value;
	time_t expiration = 0;
	long udf_flags = 0;
	char  *payload;
	size_t payload_len;
	uint32_t flags = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "dSSz|ll", &cas_d, &server_key, &key,
								  &value, &expiration, &udf_flags) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "dSz|ll", &cas_d, &key, &value,
								  &expiration, &udf_flags) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key->len == 0 || strchr(key->val, ' ')) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	DVAL_TO_LVAL(cas_d, cas);

	if (m_obj->compression) {
		MEMC_VAL_SET_FLAG(flags, MEMC_VAL_COMPRESSED);
	}

	/*
	 * php_memcached uses 16 bits internally to store type, compression and serialization info.
	 * We use 16 upper bits to store user defined flags.
	 */
	if (udf_flags > 0) {
		if ((uint32_t) udf_flags > MEMC_VAL_USER_FLAGS_MAX) {
			php_error_docref(NULL, E_WARNING, "udf_flags will be limited to %u", MEMC_VAL_USER_FLAGS_MAX);
		}
		MEMC_VAL_SET_USER_FLAGS(flags, ((uint32_t) udf_flags));
	}

	payload = php_memc_zval_to_payload(value, &payload_len, &flags, m_obj->serializer, m_obj->compression_type);
	if (payload == NULL) {
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FALSE;
	}

	if (by_key) {
		status = memcached_cas_by_key(m_obj->memc, server_key->val, server_key->len, key->val, key->len, payload, payload_len, expiration, flags, cas);
	} else {
		status = memcached_cas(m_obj->memc, key->val, key->len, payload, payload_len, expiration, flags, cas);
	}
	efree(payload);
	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::cas(double cas_token, string key, mixed value [, int expiration [, int udf_flags ] ])
   Sets the value for the given key, failing if the cas_token doesn't match the one in memcache */
PHP_METHOD(Memcached, cas)
{
	php_memc_cas_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::casByKey(double cas_token, string server_key, string key, mixed value [, int expiration [, int udf_flags ] ])
   Sets the value for the given key on the server identified by the server_key, failing if the cas_token doesn't match the one in memcache */
PHP_METHOD(Memcached, casByKey)
{
	php_memc_cas_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ Memcached::delete(string key [, int time ])
   Deletes the given key */
PHP_METHOD(Memcached, delete)
{
	php_memc_delete_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::deleteMulti(array keys [, int time ])
   Deletes the given keys */
PHP_METHOD(Memcached, deleteMulti)
{
	php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::deleteByKey(string server_key, string key [, int time ])
   Deletes the given key from the server identified by the server key */
PHP_METHOD(Memcached, deleteByKey)
{
	php_memc_delete_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ Memcached::deleteMultiByKey(array keys [, int time ])
   Deletes the given key from the server identified by the server key */
PHP_METHOD(Memcached, deleteMultiByKey)
{
	php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_delete_impl */
static void php_memc_delete_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zend_string *key, *server_key;
	time_t expiration = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|l", &server_key, &key, &expiration) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &key, &expiration) == FAILURE) {
			return;
		}
		server_key = key;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key->len == 0 || strchr(key->val, ' ')) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	if (by_key) {
		status = memcached_delete_by_key(m_obj->memc, server_key->val, server_key->len, key->val,
									 key->len, expiration);
	} else {
		status = memcached_delete(m_obj->memc, key->val, key->len, expiration);
	}

	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ -- php_memc_deleteMulti_impl */
static void php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *entries;
	zend_string *server_key = NULL;
	time_t expiration = 0;
	zval *entry;

	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa/|l", &server_key, &entries, &expiration) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "a/|l", &entries, &expiration) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	array_init(return_value);
	ZEND_HASH_FOREACH_VAL (Z_ARRVAL_P(entries), entry) {
		if (Z_TYPE_P(entry) != IS_STRING) {
			convert_to_string_ex(entry);
		}

		if (Z_STRLEN_P(entry) == 0) {
			continue;
		}

		if (by_key) {
			status = memcached_delete_by_key(m_obj->memc, server_key->val, server_key->len, Z_STRVAL_P(entry), Z_STRLEN_P(entry), expiration);
		} else {
			status = memcached_delete_by_key(m_obj->memc, Z_STRVAL_P(entry), Z_STRLEN_P(entry), Z_STRVAL_P(entry), Z_STRLEN_P(entry), expiration);
		}


		if (php_memc_handle_error(i_obj, status) < 0) {
			add_assoc_long(return_value, Z_STRVAL_P(entry), status);
		} else {
			add_assoc_bool(return_value, Z_STRVAL_P(entry), 1);
		}
	} ZEND_HASH_FOREACH_END();

	return;
}
/* }}} */

/* {{{ -- php_memc_incdec_impl */
static void php_memc_incdec_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key, zend_bool incr)
{
	zend_string *key, *server_key = NULL;
	long  offset = 1;
	uint64_t value, initial = 0;
	time_t expiry = 0;
	memcached_return status;
	int n_args = ZEND_NUM_ARGS();
	uint32_t retry = 0;

	MEMC_METHOD_INIT_VARS;

	if (!by_key) {
		if (zend_parse_parameters(n_args, "S|lll", &key, &offset, &initial, &expiry) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(n_args, "SS|lll", &server_key, &key, &offset, &initial, &expiry) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key->len == 0 || strchr(key->val, ' ')) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	if (offset < 0) {
		php_error_docref(NULL, E_WARNING, "offset has to be > 0");
		RETURN_FALSE;
	}

retry:
	if ((!by_key && n_args < 3) || (by_key && n_args < 4)) {
		if (by_key) {
			if (incr) {
				status = memcached_increment_by_key(m_obj->memc, server_key->val, server_key->len, key->val, key->len, (unsigned int)offset, &value);
			} else {
				status = memcached_decrement_by_key(m_obj->memc, server_key->val, server_key->len, key->val, key->len, (unsigned int)offset, &value);
			}
		} else {
			if (incr) {
				status = memcached_increment(m_obj->memc, key->val, key->len, (unsigned int)offset, &value);
			} else {
				status = memcached_decrement(m_obj->memc, key->val, key->len, (unsigned int)offset, &value);
			}
		}
	} else {
		if (!memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL)) {
			php_error_docref(NULL, E_WARNING, "Initial value is only supported with binary protocol");
			RETURN_FALSE;
		}
		if (by_key) {
			if (incr) {
				status = memcached_increment_with_initial_by_key(m_obj->memc, server_key->val, server_key->len, key->val, key->len, (unsigned int)offset, initial, expiry, &value);
			} else {
				status = memcached_decrement_with_initial_by_key(m_obj->memc, server_key->val, server_key->len, key->val, key->len, (unsigned int)offset, initial, expiry, &value);
			}
		} else {
			if (incr) {
				status = memcached_increment_with_initial(m_obj->memc, key->val, key->len, (unsigned int)offset, initial, expiry, &value);
			} else {
				status = memcached_decrement_with_initial(m_obj->memc, key->val, key->len, (unsigned int)offset, initial, expiry, &value);
			}
		}
	}

	if (php_memc_handle_error(i_obj, status) < 0) {
		PHP_MEMC_FAILOVER_RETRY
		RETURN_FALSE;
	}

	RETURN_LONG((long)value);
}
/* }}} */

/* {{{ Memcached::increment(string key [, int delta [, initial_value [, expiry time ] ] ])
   Increments the value for the given key by delta, defaulting to 1 */
PHP_METHOD(Memcached, increment)
{
	php_memc_incdec_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 1);
}
/* }}} */

/* {{{ Memcached::decrement(string key [, int delta [, initial_value [, expiry time ] ] ])
   Decrements the value for the given key by delta, defaulting to 1 */
PHP_METHOD(Memcached, decrement)
{
	php_memc_incdec_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

/* {{{ Memcached::decrementByKey(string server_key, string key [, int delta [, initial_value [, expiry time ] ] ])
   Decrements by server the value for the given key by delta, defaulting to 1 */
PHP_METHOD(Memcached, decrementByKey)
{
	php_memc_incdec_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */

/* {{{ Memcached::increment(string server_key, string key [, int delta [, initial_value [, expiry time ] ] ])
   Increments by server the value for the given key by delta, defaulting to 1 */
PHP_METHOD(Memcached, incrementByKey)
{
	php_memc_incdec_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 1);
}
/* }}} */

/* {{{ Memcached::addServer(string hostname, int port [, int weight ])
   Adds the given memcache server to the list */
PHP_METHOD(Memcached, addServer)
{
	zend_string *host;
	long  port, weight = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sl|l", &host, &port, &weight) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX < 0x01000002
	if (host->val[0] == '/') { /* unix domain socket */
		status = memcached_server_add_unix_socket_with_weight(m_obj->memc, host->val, weight);
	} else if (memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_USE_UDP)) {
		status = memcached_server_add_udp_with_weight(m_obj->memc, host->val, port, weight);
	} else {
		status = memcached_server_add_with_weight(m_obj->memc, host->val, port, weight);
	}
#else
	status = memcached_server_add_with_weight(m_obj->memc, host->val, port, weight);
#endif

	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::addServers(array servers)
   Adds the given memcache servers to the server list */
PHP_METHOD(Memcached, addServers)
{
	zval *servers;
	zval *entry;
	zval *z_host, *z_port, *z_weight = NULL;
	uint32_t weight = 0;
	HashPosition	pos;
	int   entry_size, i = 0;
	memcached_server_st *list = NULL;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a/", &servers) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	ZEND_HASH_FOREACH_VAL (Z_ARRVAL_P(servers), entry) {
		if (Z_TYPE_P(entry) != IS_ARRAY) {
			php_error_docref(NULL, E_WARNING, "server list entry #%d is not an array", i+1);
			i++;
			continue;
		}

		entry_size = zend_hash_num_elements(Z_ARRVAL_P(entry));

		if (entry_size > 1) {
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(entry), &pos);

			/* Check that we have a host */
			if ((z_host = zend_hash_get_current_data_ex(Z_ARRVAL_P(entry), &pos)) == NULL) {
				php_error_docref(NULL, E_WARNING, "could not get server host for entry #%d", i+1);
				i++;
				continue;
			}

			/* Check that we have a port */
			if (zend_hash_move_forward_ex(Z_ARRVAL_P(entry), &pos) == FAILURE ||
			    (z_port = zend_hash_get_current_data_ex(Z_ARRVAL_P(entry), &pos)) == NULL) {
				php_error_docref(NULL, E_WARNING, "could not get server port for entry #%d", i+1);
				i++;
				continue;
			}

			convert_to_string_ex(z_host);
			convert_to_long_ex(z_port);

			weight = 0;
			if (entry_size > 2) {
				/* Try to get weight */
				if (zend_hash_move_forward_ex(Z_ARRVAL_P(entry), &pos) == FAILURE ||
					(z_weight = zend_hash_get_current_data_ex(Z_ARRVAL_P(entry), &pos)) == NULL) {
					php_error_docref(NULL, E_WARNING, "could not get server weight for entry #%d", i+1);
				}

				convert_to_long_ex(z_weight);
				weight = Z_LVAL_P(z_weight);
			}

			list = memcached_server_list_append_with_weight(list, Z_STRVAL_P(z_host),
				Z_LVAL_P(z_port), weight, &status);

			if (php_memc_handle_error(i_obj, status) == 0) {
				i++;
				continue;
			}
		}
		i++;
		/* catch-all for all errors */
		php_error_docref(NULL, E_WARNING, "could not add entry #%d to the server list", i+1);
	} ZEND_HASH_FOREACH_END();

	status = memcached_server_push(m_obj->memc, list);
	memcached_server_list_free(list);
	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::getServerList()
   Returns the list of the memcache servers in use */
PHP_METHOD(Memcached, getServerList)
{
	struct callbackContext context = {0};
	memcached_server_function callbacks[1];
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	callbacks[0] = php_memc_do_serverlist_callback;
	array_init(return_value);
	context.return_value = return_value;
	memcached_server_cursor(m_obj->memc, callbacks, &context, 1);
}
/* }}} */

/* {{{ Memcached::getServerByKey(string server_key)
   Returns the server identified by the given server key */
PHP_METHOD(Memcached, getServerByKey)
{
	zend_string *server_key;
	php_memcached_instance_st server_instance;
	memcached_return error;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &server_key) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (server_key->len == 0 || strchr(server_key->val, ' ')) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	server_instance = memcached_server_by_key(m_obj->memc, server_key->val, server_key->len, &error);
	if (server_instance == NULL) {
		php_memc_handle_error(i_obj, error);
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string(return_value, "host", (char*) memcached_server_name(server_instance));
	add_assoc_long(return_value, "port", memcached_server_port(server_instance));
	add_assoc_long(return_value, "weight", 0);
}
/* }}} */

/* {{{ Memcached::resetServerList()
   Reset the server list in use */
PHP_METHOD(Memcached, resetServerList)
{
    MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

    MEMC_METHOD_FETCH_OBJECT;

    memcached_servers_reset(m_obj->memc);
    RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::quit()
   Close any open connections */
PHP_METHOD(Memcached, quit)
{
    MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

    MEMC_METHOD_FETCH_OBJECT;

    memcached_quit(m_obj->memc);
    RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::flushBuffers()
   Flush and senf buffered commands */
PHP_METHOD(Memcached, flushBuffers)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

    MEMC_METHOD_FETCH_OBJECT;
	RETURN_BOOL(memcached_flush_buffers(m_obj->memc) == MEMCACHED_SUCCESS);
}
/* }}} */

#ifdef HAVE_LIBMEMCACHED_CHECK_CONFIGURATION
/* {{{ Memcached::getLastErrorMessage()
   Returns the last error message that occurred */
PHP_METHOD(Memcached, getLastErrorMessage)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_STRING(memcached_last_error_message(m_obj->memc));
}
/* }}} */

/* {{{ Memcached::getLastErrorCode()
   Returns the last error code that occurred */
PHP_METHOD(Memcached, getLastErrorCode)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_LONG(memcached_last_error(m_obj->memc));
}
/* }}} */

/* {{{ Memcached::getLastErrorErrno()
   Returns the last error errno that occurred */
PHP_METHOD(Memcached, getLastErrorErrno)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_LONG(memcached_last_error_errno(m_obj->memc));
}
/* }}} */
#endif

/* {{{ Memcached::getLastDisconnectedServer()
   Returns the last disconnected server
   Was added in 0.34 according to libmemcached's Changelog */
PHP_METHOD(Memcached, getLastDisconnectedServer)
{
	php_memcached_instance_st server_instance;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	server_instance = memcached_server_get_last_disconnect(m_obj->memc);
	if (server_instance == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string(return_value, "host", (char*) memcached_server_name(server_instance));
	add_assoc_long(return_value, "port", memcached_server_port(server_instance));
}
/* }}} */

/* {{{ Memcached::getStats()
   Returns statistics for the memcache servers */
PHP_METHOD(Memcached, getStats)
{
	memcached_stat_st *stats;
	memcached_return status;
	struct callbackContext context = {0};
	memcached_server_function callbacks[1];
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	if (memcached_server_count(m_obj->memc) == 0) {
		array_init(return_value);
		return;
	}

	stats = memcached_stat(m_obj->memc, NULL, &status);
	php_memc_handle_error(i_obj, status);
	if (stats == NULL) {
		RETURN_FALSE;
	} else if (status != MEMCACHED_SUCCESS && status != MEMCACHED_SOME_ERRORS) {
		memcached_stat_free(m_obj->memc, stats);
		RETURN_FALSE;
	}

	array_init(return_value);

	callbacks[0] = php_memc_do_stats_callback;
	context.i = 0;
	context.stats = stats;
	context.return_value = return_value;
	memcached_server_cursor(m_obj->memc, callbacks, &context, 1);

	memcached_stat_free(m_obj->memc, stats);
}
/* }}} */

/* {{{ Memcached::getVersion()
   Returns the version of each memcached server in the pool */
PHP_METHOD(Memcached, getVersion)
{
	memcached_return status = MEMCACHED_SUCCESS;
	struct callbackContext context = {0};
	memcached_server_function callbacks[1];
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);

	status = memcached_version(m_obj->memc);
	if (php_memc_handle_error(i_obj, status) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}

	callbacks[0] = php_memc_do_version_callback;
	context.return_value = return_value;

	memcached_server_cursor(m_obj->memc, callbacks, &context, 1);
}
/* }}} */

/* {{{ Memcached::getAllKeys()
	Returns the keys stored on all the servers */
static memcached_return php_memc_dump_func_callback(const memcached_st *ptr __attribute__((unused)), \
	const char *key, size_t key_length, void *context)
{
	zval *ctx = (zval*) context;
	add_next_index_string(ctx, (char*) key);

	return MEMCACHED_SUCCESS;
}

PHP_METHOD(Memcached, getAllKeys)
{
	memcached_return rc;
	memcached_dump_func callback[1];
	MEMC_METHOD_INIT_VARS;

	callback[0] = php_memc_dump_func_callback;
	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);
	rc = memcached_dump(m_obj->memc, callback, return_value, 1);
	if (php_memc_handle_error(i_obj, rc) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::flush([ int delay ])
   Flushes the data on all the servers */
static PHP_METHOD(Memcached, flush)
{
	time_t delay = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &delay) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	status = memcached_flush(m_obj->memc, delay);
	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::getOption(int option)
   Returns the value for the given option constant */
static PHP_METHOD(Memcached, getOption)
{
	long option;
	uint64_t result;
	memcached_behavior flag;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &option) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	switch (option) {
		case MEMC_OPT_COMPRESSION_TYPE:
			RETURN_LONG(m_obj->compression_type);

		case MEMC_OPT_COMPRESSION:
			RETURN_BOOL(m_obj->compression);

		case MEMC_OPT_PREFIX_KEY:
		{
			memcached_return retval;
			char *result;

			result = memcached_callback_get(m_obj->memc, MEMCACHED_CALLBACK_PREFIX_KEY, &retval);
			if (retval == MEMCACHED_SUCCESS && result) {
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX == 0x00049000
				RETURN_STRINGL(result, strlen(result));
#else
				RETURN_STRING(result);
#endif
			} else {
				RETURN_EMPTY_STRING();
			}
		}

		case MEMC_OPT_SERIALIZER:
			RETURN_LONG((long)m_obj->serializer);
			break;

		case MEMC_OPT_STORE_RETRY_COUNT:
			RETURN_LONG((long)m_obj->store_retry_count);
			break;

		case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
		case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
			if (memcached_server_count(m_obj->memc) == 0) {
				php_error_docref(NULL, E_WARNING, "no servers defined");
				return;
			}

		default:
			/*
			 * Assume that it's a libmemcached behavior option.
			 */
			flag = (memcached_behavior) option;
			result = memcached_behavior_get(m_obj->memc, flag);
			RETURN_LONG((long)result);
	}
}
/* }}} */

static int php_memc_set_option(php_memc_t *i_obj, long option, zval *value)
{
	memcached_return rc = MEMCACHED_FAILURE;
	memcached_behavior flag;
	struct memc_obj *m_obj = i_obj->obj;

	switch (option) {
		case MEMC_OPT_COMPRESSION:
			convert_to_long(value);
			m_obj->compression = Z_LVAL_P(value) ? 1 : 0;
			break;

		case MEMC_OPT_COMPRESSION_TYPE:
			convert_to_long(value);
			if (Z_LVAL_P(value) == COMPRESSION_TYPE_FASTLZ ||
				Z_LVAL_P(value) == COMPRESSION_TYPE_ZLIB) {
				m_obj->compression_type = Z_LVAL_P(value);
			} else {
				/* invalid compression type */
				i_obj->rescode = MEMCACHED_INVALID_ARGUMENTS;
				return 0;
			}
			break;

		case MEMC_OPT_PREFIX_KEY:
		{
			char *key;
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX == 0x00049000
			char tmp[MEMCACHED_PREFIX_KEY_MAX_SIZE - 1];
#endif
			convert_to_string(value);
			if (Z_STRLEN_P(value) == 0) {
				key = NULL;
			} else {
				/*
				   work-around a bug in libmemcached in version 0.49 that truncates the trailing
				   character of the key prefix, to avoid the issue we pad it with a '0'
				*/
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX == 0x00049000
				snprintf(tmp, sizeof(tmp), "%s0", Z_STRVAL_P(value));
				key = tmp;
#else
				key = Z_STRVAL_P(value);
#endif
			}
			if (memcached_callback_set(m_obj->memc, MEMCACHED_CALLBACK_PREFIX_KEY, key) == MEMCACHED_BAD_KEY_PROVIDED) {
				i_obj->rescode = MEMCACHED_INVALID_ARGUMENTS;
				php_error_docref(NULL, E_WARNING, "bad key provided");
				return 0;
			}
		}
			break;

		case MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED:
			flag = (memcached_behavior) option;

			convert_to_long(value);
			rc = memcached_behavior_set(m_obj->memc, flag, (uint64_t) Z_LVAL_P(value));

			if (php_memc_handle_error(i_obj, rc) < 0) {
				php_error_docref(NULL, E_WARNING, "error setting memcached option: %s", memcached_strerror (m_obj->memc, rc));
				return 0;
			}

			/*
			 * This is necessary because libmemcached does not reset hash/distribution
			 * options on false case, like it does for MEMCACHED_BEHAVIOR_KETAMA
			 * (non-weighted) case. We have to clean up ourselves.
			 */
			if (!Z_LVAL_P(value)) {
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX > 0x00037000
			(void)memcached_behavior_set_key_hash(m_obj->memc, MEMCACHED_HASH_DEFAULT);
			(void)memcached_behavior_set_distribution_hash(m_obj->memc, MEMCACHED_HASH_DEFAULT);
			(void)memcached_behavior_set_distribution(m_obj->memc, MEMCACHED_DISTRIBUTION_MODULA);
#else
				m_obj->memc->hash = 0;
				m_obj->memc->distribution = 0;
#endif
			}
			break;

		case MEMC_OPT_SERIALIZER:
		{
			convert_to_long(value);
			/* igbinary serializer */
#ifdef HAVE_MEMCACHED_IGBINARY
			if (Z_LVAL_P(value) == SERIALIZER_IGBINARY) {
				m_obj->serializer = SERIALIZER_IGBINARY;
			} else
#endif
#ifdef HAVE_JSON_API
			if (Z_LVAL_P(value) == SERIALIZER_JSON) {
				m_obj->serializer = SERIALIZER_JSON;
			} else if (Z_LVAL_P(value) == SERIALIZER_JSON_ARRAY) {
				m_obj->serializer = SERIALIZER_JSON_ARRAY;
			} else
#endif
			/* msgpack serializer */
#ifdef HAVE_MEMCACHED_MSGPACK
			if (Z_LVAL_P(value) == SERIALIZER_MSGPACK) {
				m_obj->serializer = SERIALIZER_MSGPACK;
			} else
#endif
			/* php serializer */
			if (Z_LVAL_P(value) == SERIALIZER_PHP) {
				m_obj->serializer = SERIALIZER_PHP;
			} else {
				m_obj->serializer = SERIALIZER_PHP;
				i_obj->rescode = MEMCACHED_INVALID_ARGUMENTS;
				php_error_docref(NULL, E_WARNING, "invalid serializer provided");
				return 0;
			}
			break;
		}

		case MEMC_OPT_STORE_RETRY_COUNT:
			convert_to_long(value);
			m_obj->store_retry_count = Z_LVAL_P(value);
			break;

		default:
			/*
			 * Assume that it's a libmemcached behavior option.
			 */
			if (option < 0) {
				rc = MEMCACHED_INVALID_ARGUMENTS;
			}
			else {
				flag = (memcached_behavior) option;
				convert_to_long(value);

				if (flag < MEMCACHED_BEHAVIOR_MAX) {
					rc = memcached_behavior_set(m_obj->memc, flag, (uint64_t) Z_LVAL_P(value));
				}
				else {
					rc = MEMCACHED_INVALID_ARGUMENTS;
				}
			}

			if (php_memc_handle_error(i_obj, rc) < 0) {
				php_error_docref(NULL, E_WARNING, "error setting memcached option: %s", memcached_strerror (m_obj->memc, rc));
				return 0;
			}
			break;
	}
	return 1;
}

static
uint32_t *s_zval_to_uint32_array (zval *input, size_t *num_elements)
{
	zval *pzval;
	uint32_t *retval;
	size_t i = 0;

	*num_elements = zend_hash_num_elements(Z_ARRVAL_P(input));

	if (!*num_elements) {
		return NULL;
	}

	retval = ecalloc(*num_elements, sizeof(uint32_t));

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(input), pzval) {
		long value = 0;

		if (Z_TYPE_P(pzval) == IS_LONG) {
			value = Z_LVAL_P(pzval);
		} else {
			value = zval_get_long(pzval);
			value = value > 0? value : 0;
		}

		if (value < 0) {
			php_error_docref(NULL, E_WARNING, "the map must contain positive integers");
			efree (retval);
			*num_elements = 0;
			return NULL;
		}
		retval [i] = (uint32_t) value;
		i++;
	} ZEND_HASH_FOREACH_END();
	return retval;
}

/* {{{ Memcached::setBucket(array host_map, array forward_map, integer replicas)
   Sets the memcached virtual buckets */

PHP_METHOD(Memcached, setBucket)
{
	zval *zserver_map;
	zval *zforward_map = NULL;
	long replicas = 0;
	zend_bool retval = 1;

	uint32_t *server_map = NULL, *forward_map = NULL;
	size_t server_map_len = 0, forward_map_len = 0;
	memcached_return rc;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "aa!l", &zserver_map, &zforward_map, &replicas) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	if (zend_hash_num_elements (Z_ARRVAL_P(zserver_map)) == 0) {
		php_error_docref(NULL, E_WARNING, "server map cannot be empty");
		RETURN_FALSE;
	}

	if (zforward_map && zend_hash_num_elements (Z_ARRVAL_P(zserver_map)) != zend_hash_num_elements (Z_ARRVAL_P(zforward_map))) {
		php_error_docref(NULL, E_WARNING, "forward_map length must match the server_map length");
		RETURN_FALSE;
	}

	if (replicas < 0) {
		php_error_docref(NULL, E_WARNING, "replicas must be larger than zero");
		RETURN_FALSE;
	}

	server_map = s_zval_to_uint32_array (zserver_map, &server_map_len);

	if (!server_map) {
		RETURN_FALSE;
	}

	if (zforward_map) {
		forward_map = s_zval_to_uint32_array (zforward_map, &forward_map_len);

		if (!forward_map) {
			efree (server_map);
			RETURN_FALSE;
		}
	}

	rc = memcached_bucket_set (m_obj->memc, server_map, forward_map, (uint32_t) server_map_len, replicas);

	if (php_memc_handle_error(i_obj, rc) < 0) {
		retval = 0;;
	}

	efree(server_map);

	if (forward_map) {
		efree(forward_map);
	}
	RETURN_BOOL(retval);
}
/* }}} */

/* {{{ Memcached::setOptions(array options)
   Sets the value for the given option constant */
static PHP_METHOD(Memcached, setOptions)
{
	zval *options;
	zend_bool ok = 1;
	zend_string *key;
	ulong key_index;
	zval *value;

	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &options) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), key_index, key, value) {
		if (key) {
			php_error_docref(NULL, E_WARNING, "invalid configuration option");
			ok = 0;
		} else {
			zval copy;
			ZVAL_DUP(&copy, value);

			if (!php_memc_set_option(i_obj, (long) key_index, &copy)) {
				ok = 0;
			}

			zval_dtor(&copy);
		}
	} ZEND_HASH_FOREACH_END();

	RETURN_BOOL(ok);
}
/* }}} */

/* {{{ Memcached::setOption(int option, mixed value)
   Sets the value for the given option constant */
static PHP_METHOD(Memcached, setOption)
{
	long option;
	zval *value;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz/", &option, &value) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_BOOL(php_memc_set_option(i_obj, option, value));
}
/* }}} */

#ifdef HAVE_MEMCACHED_SASL
/* {{{ Memcached::setSaslAuthData(string user, string pass)
   Sets sasl credentials */
static PHP_METHOD(Memcached, setSaslAuthData)
{
	MEMC_METHOD_INIT_VARS;
	memcached_return status;
	zend_string *user, *pass;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &user, &pass) == FAILURE) {
		return;
	}

	if (!MEMC_G(use_sasl)) {
		php_error_docref(NULL, E_WARNING, "SASL support (memcached.use_sasl) isn't enabled in php.ini");
		RETURN_FALSE;
	}

	MEMC_METHOD_FETCH_OBJECT;

	if (!memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL)) {
		php_error_docref(NULL, E_WARNING, "SASL is only supported with binary protocol");
		RETURN_FALSE;
	}
	m_obj->has_sasl_data = 1;
	status = memcached_set_sasl_auth_data(m_obj->memc, user->val, pass->val);

	if (php_memc_handle_error(i_obj, status) < 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */
#endif /* HAVE_MEMCACHED_SASL */

/* {{{ Memcached::getResultCode()
   Returns the result code from the last operation */
static PHP_METHOD(Memcached, getResultCode)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_LONG(i_obj->rescode);
}
/* }}} */

/* {{{ Memcached::getResultMessage()
   Returns the result message from the last operation */
static PHP_METHOD(Memcached, getResultMessage)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	switch (i_obj->rescode) {
		case MEMC_RES_PAYLOAD_FAILURE:
			RETURN_STRING("PAYLOAD FAILURE");
			break;

		case MEMCACHED_ERRNO:
		case MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE:
		case MEMCACHED_UNKNOWN_READ_FAILURE:
			if (i_obj->memc_errno) {
				zend_string *str = strpprintf(0, "%s: %s",
						memcached_strerror(m_obj->memc, (memcached_return)i_obj->rescode), strerror(i_obj->memc_errno));
				RETURN_STR(str);
			}
			/* Fall through */
		default:
			RETURN_STRING(memcached_strerror(m_obj->memc, (memcached_return)i_obj->rescode));
			break;
	}

}
/* }}} */

/* {{{ Memcached::isPersistent()
   Returns the true if instance uses a persistent connection */
static PHP_METHOD(Memcached, isPersistent)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_BOOL(i_obj->is_persistent);
}
/* }}} */

/* {{{ Memcached::isPristine()
   Returns the true if instance is recently created */
static PHP_METHOD(Memcached, isPristine)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_BOOL(i_obj->is_pristine);
}
/* }}} */

/****************************************
  Internal support code
****************************************/

/* {{{ constructor/destructor */
static void php_memc_destroy(struct memc_obj *m_obj, zend_bool persistent)
{
#if HAVE_MEMCACHED_SASL
	if (m_obj->has_sasl_data) {
		memcached_destroy_sasl_auth_data(m_obj->memc);
	}
#endif
	if (m_obj->memc) {
		memcached_free(m_obj->memc);
	}

	pefree(m_obj, persistent);
}

static void php_memc_free_storage(zend_object *obj)
{
	php_memc_t *i_obj = php_memc_fetch_object(obj);
	
	if (i_obj->obj && !i_obj->is_persistent) {
		php_memc_destroy(i_obj->obj, 0);
	}

	zend_object_std_dtor(&i_obj->zo);
	i_obj->obj = NULL;
}

zend_object *php_memc_new(zend_class_entry *ce)
{
	php_memc_t *i_obj = ecalloc(1, sizeof(php_memc_t) + zend_object_properties_size(ce));
    
	zend_object_std_init(&i_obj->zo, ce);
	object_properties_init(&i_obj->zo, ce);

	i_obj->zo.handlers = &memcached_object_handlers;
    
	return &i_obj->zo;
}

#ifdef HAVE_MEMCACHED_PROTOCOL
static
void php_memc_server_free_storage(php_memc_server_t *intern)
{
	zend_object_std_dtor(&intern->zo);
	efree (intern);
}

zend_object_value php_memc_server_new(zend_class_entry *ce)
{
	zend_object_value retval;
	php_memc_server_t *intern;
	zval *tmp;

	intern = ecalloc(1, sizeof(php_memc_server_t));
	zend_object_std_init(&intern->zo, ce);
	object_properties_init(&intern->zo, ce);

	intern->handler = php_memc_proto_handler_new ();

	retval.handle = zend_objects_store_put(intern, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t)php_memc_server_free_storage, NULL);
	retval.handlers = &memcached_server_object_handlers;

	return retval;
}
#endif

ZEND_RSRC_DTOR_FUNC(php_memc_dtor)
{
	if (res->ptr) {
		struct memc_obj *m_obj = (struct memc_obj *)res->ptr;
		php_memc_destroy(m_obj, 1);
		res->ptr = NULL;
	}
}

ZEND_RSRC_DTOR_FUNC(php_memc_sess_dtor)
{
	if (res->ptr) {
		memcached_sess *memc_sess = (memcached_sess *)res->ptr;
		memcached_free(memc_sess->memc_sess);
		pefree(res->ptr, 1);
		res->ptr = NULL;
	}
}
/* }}} */

/* {{{ internal API functions */
static memcached_return php_memc_do_serverlist_callback(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context)
{
	struct callbackContext* context = (struct callbackContext*) in_context;
	zval array;

	array_init(&array);
	add_assoc_string(&array, "host", (char*) memcached_server_name(instance));
	add_assoc_long(&array, "port", memcached_server_port(instance));
	/*
	 * API does not allow to get at this field.
	add_assoc_long(array, "weight", instance->weight);
	*/

	add_next_index_zval(context->return_value, &array);
	return MEMCACHED_SUCCESS;
}

static memcached_return php_memc_do_stats_callback(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context)
{
	char *hostport = NULL;
	int hostport_len;
	struct callbackContext* context = (struct callbackContext*) in_context;
	zval entry;
	hostport_len = spprintf(&hostport, 0, "%s:%d", memcached_server_name(instance), memcached_server_port(instance));

	array_init(&entry);

	add_assoc_long(&entry, "pid", context->stats[context->i].pid);
	add_assoc_long(&entry, "uptime", context->stats[context->i].uptime);
	add_assoc_long(&entry, "threads", context->stats[context->i].threads);
	add_assoc_long(&entry, "time", context->stats[context->i].time);
	add_assoc_long(&entry, "pointer_size", context->stats[context->i].pointer_size);
	add_assoc_long(&entry, "rusage_user_seconds", context->stats[context->i].rusage_user_seconds);
	add_assoc_long(&entry, "rusage_user_microseconds", context->stats[context->i].rusage_user_microseconds);
	add_assoc_long(&entry, "rusage_system_seconds", context->stats[context->i].rusage_system_seconds);
	add_assoc_long(&entry, "rusage_system_microseconds", context->stats[context->i].rusage_system_microseconds);
	add_assoc_long(&entry, "curr_items", context->stats[context->i].curr_items);
	add_assoc_long(&entry, "total_items", context->stats[context->i].total_items);
	add_assoc_long(&entry, "limit_maxbytes", context->stats[context->i].limit_maxbytes);
	add_assoc_long(&entry, "curr_connections", context->stats[context->i].curr_connections);
	add_assoc_long(&entry, "total_connections", context->stats[context->i].total_connections);
	add_assoc_long(&entry, "connection_structures", context->stats[context->i].connection_structures);
	add_assoc_long(&entry, "bytes", context->stats[context->i].bytes);
	add_assoc_long(&entry, "cmd_get", context->stats[context->i].cmd_get);
	add_assoc_long(&entry, "cmd_set", context->stats[context->i].cmd_set);
	add_assoc_long(&entry, "get_hits", context->stats[context->i].get_hits);
	add_assoc_long(&entry, "get_misses", context->stats[context->i].get_misses);
	add_assoc_long(&entry, "evictions", context->stats[context->i].evictions);
	add_assoc_long(&entry, "bytes_read", context->stats[context->i].bytes_read);
	add_assoc_long(&entry, "bytes_written", context->stats[context->i].bytes_written);
	add_assoc_stringl(&entry, "version", context->stats[context->i].version, strlen(context->stats[context->i].version));

	add_assoc_zval_ex(context->return_value, hostport, hostport_len, &entry);
	efree(hostport);

	/* Increment the server count in our context structure. Failure to do so will cause only the stats for the last server to get displayed. */
	context->i++;
	return MEMCACHED_SUCCESS;
}

static memcached_return php_memc_do_version_callback(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context)
{
	char *hostport = NULL;
	char version[16];
	int hostport_len, version_len;
	struct callbackContext* context = (struct callbackContext*) in_context;

	hostport_len = spprintf(&hostport, 0, "%s:%d", memcached_server_name(instance), memcached_server_port(instance) - 1);
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000009
	version_len = snprintf(version, sizeof(version), "%d.%d.%d",
				memcached_server_major_version(instance),
				memcached_server_minor_version(instance),
				memcached_server_micro_version(instance));
#else
	version_len = snprintf(version, sizeof(version), "%d.%d.%d",
				instance->major_version,
				instance->minor_version,
				instance->micro_version);
#endif

	add_assoc_stringl_ex(context->return_value, hostport, hostport_len, version, version_len);
	efree(hostport);
	return MEMCACHED_SUCCESS;
}

static int php_memc_handle_error(php_memc_t *i_obj, memcached_return status)
{
	int result = 0;

	switch (status) {
		case MEMCACHED_SUCCESS:
		case MEMCACHED_STORED:
		case MEMCACHED_DELETED:
		case MEMCACHED_STAT:
			result = 0;
			i_obj->memc_errno = 0;
			break;

		case MEMCACHED_END:
		case MEMCACHED_BUFFERED:
			i_obj->rescode = status;
			i_obj->memc_errno = 0;
			result = 0;
			break;

		case MEMCACHED_SOME_ERRORS:
			i_obj->rescode = status;
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00049000
			i_obj->memc_errno = memcached_last_error_errno(i_obj->obj->memc);
#else
			i_obj->memc_errno = i_obj->obj->memc->cached_errno;	/* Hnngghgh! */

#endif
			result = 0;
			break;

		default:
			i_obj->rescode = status;
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00049000
			i_obj->memc_errno = memcached_last_error_errno(i_obj->obj->memc);
#else
			i_obj->memc_errno = i_obj->obj->memc->cached_errno; /* Hnngghgh! */

#endif
			result = -1;
			break;
	}

	return result;
}

static
char *s_compress_value (enum memcached_compression_type compression_type, const char *payload, size_t *payload_len, uint32_t *flags)
{
	/* status */
	zend_bool compress_status = 0;

	/* Additional 5% for the data */
	size_t buffer_size = (size_t) (((double) *payload_len * 1.05) + 1.0);
	char *buffer = emalloc(sizeof(uint32_t) + buffer_size);

	/* Store compressed size here */
	size_t compressed_size = 0;
    uint32_t plen = *payload_len;

	/* Copy the uin32_t at the beginning */
	memcpy(buffer, &plen, sizeof(uint32_t));
	buffer += sizeof(uint32_t);

	switch (compression_type) {

		case COMPRESSION_TYPE_FASTLZ:
			compress_status = ((compressed_size = fastlz_compress(payload, *payload_len, buffer)) > 0);
			MEMC_VAL_SET_FLAG(*flags, MEMC_VAL_COMPRESSION_FASTLZ);
			break;

		case COMPRESSION_TYPE_ZLIB:
			/* ZLIB returns the compressed size in this buffer */
			compressed_size = buffer_size;

			compress_status = (compress((Bytef *)buffer, &compressed_size, (Bytef *)payload, *payload_len) == Z_OK);
			MEMC_VAL_SET_FLAG(*flags, MEMC_VAL_COMPRESSION_ZLIB);
			break;

		default:
			compress_status = 0;
			break;
	}
	buffer -= sizeof(uint32_t);
	*payload_len = compressed_size + sizeof(uint32_t);

	if (!compress_status) {
		php_error_docref(NULL, E_WARNING, "could not compress value");
		MEMC_VAL_DEL_FLAG(*flags, MEMC_VAL_COMPRESSED);

		efree (buffer);
		*payload_len = 0;
		return NULL;
	}

	else if (*payload_len > (compressed_size * MEMC_G(compression_factor))) {
		MEMC_VAL_DEL_FLAG(*flags, MEMC_VAL_COMPRESSED);
		efree (buffer);
		*payload_len = 0;
		return NULL;
	}
	return buffer;
}

static
zend_bool s_serialize_value (enum memcached_serializer serializer, zval *value, smart_str *buf, uint32_t *flags)
{
	switch (serializer) {

		/*
			Igbinary serialization
		*/
#ifdef HAVE_MEMCACHED_IGBINARY
		case SERIALIZER_IGBINARY:
			if (igbinary_serialize((uint8_t **) &(buf->s), &buf->s->len, value) != 0) {
				php_error_docref(NULL, E_WARNING, "could not serialize value with igbinary");
				return 0;
			}
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_IGBINARY);
			break;
#endif

		/*
			JSON serialization
		*/
#ifdef HAVE_JSON_API
		case SERIALIZER_JSON:
		case SERIALIZER_JSON_ARRAY:
		{
			php_json_encode(buf, value, 0);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_JSON);
		}
			break;
#endif

		/*
			msgpack serialization
		*/
#ifdef HAVE_MEMCACHED_MSGPACK
		case SERIALIZER_MSGPACK:
			php_msgpack_serialize(buf, value);
			if (!buf->s) {
				php_error_docref(NULL, E_WARNING, "could not serialize value with msgpack");
				return 0;
			}
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_MSGPACK);
			break;
#endif

		/*
			PHP serialization
		*/
		default:
		{
			php_serialize_data_t var_hash;
			PHP_VAR_SERIALIZE_INIT(var_hash);
			php_var_serialize(buf, value, &var_hash);
			PHP_VAR_SERIALIZE_DESTROY(var_hash);

			if (!buf->s) {
				php_error_docref(NULL, E_WARNING, "could not serialize value");
				return 0;
			}
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_SERIALIZED);
		}
			break;
	}

	/* Check for exceptions caused by serializers */
	if (EG(exception) && buf->s->len) {
		return 0;
	}
	return 1;
}

static
char *php_memc_zval_to_payload(zval *value, size_t *payload_len, uint32_t *flags, enum memcached_serializer serializer, enum memcached_compression_type compression_type)
{
	const char *pl;
	size_t pl_len = 0;

	char *payload = NULL;
	smart_str buf = {0};
	char tmp[40] = {0};

	switch (Z_TYPE_P(value)) {

		case IS_STRING:
			pl = Z_STRVAL_P(value);
			pl_len = Z_STRLEN_P(value);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_STRING);
			break;

		case IS_LONG:
			pl_len = sprintf(tmp, "%ld", Z_LVAL_P(value));
			pl = tmp;
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_LONG);
			break;

		case IS_DOUBLE:
			php_memcached_g_fmt(tmp, Z_DVAL_P(value));
			pl = tmp;
			pl_len = strlen(tmp);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_DOUBLE);
			break;

		case IS_TRUE:
			pl_len = 1;
			tmp[0] = '1';
			tmp[1] = '\0';
			pl = tmp;
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_BOOL);
			break;

		case IS_FALSE:
			pl_len = 0;
			tmp[0] = '\0';
			pl = tmp;
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_BOOL);
			break;

		default:
			if (!s_serialize_value (serializer, value, &buf, flags)) {
				smart_str_free(&buf);
				return NULL;
			}
			pl = buf.s->val;
			pl_len = buf.s->len;
			break;
	}

	/* turn off compression for values below the threshold */
	if (MEMC_VAL_HAS_FLAG(*flags, MEMC_VAL_COMPRESSED) && pl_len < MEMC_G(compression_threshold)) {
		MEMC_VAL_DEL_FLAG(*flags, MEMC_VAL_COMPRESSED);
	}

	/* If we have compression flag, compress the value */
	if (MEMC_VAL_HAS_FLAG(*flags, MEMC_VAL_COMPRESSED)) {
		/* status */
		*payload_len = pl_len;
		payload = s_compress_value (compression_type, pl, payload_len, flags);
	}

	/* If compression failed or value is below threshold we just use plain value */
	if (!payload || !MEMC_VAL_HAS_FLAG(*flags, MEMC_VAL_COMPRESSED)) {
		*payload_len = (uint32_t) pl_len;
		payload      = estrndup(pl, pl_len);
	}

	if (buf.s) {
		smart_str_free(&buf);
	}
	return payload;
}

static
char *s_decompress_value (const char *payload, size_t *payload_len, uint32_t flags)
{
	char *buffer = NULL;
	uint32_t len;
	unsigned long length;
	zend_bool decompress_status = 0;

	/* Stored with newer memcached extension? */
	if (MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSION_FASTLZ) || MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSION_ZLIB)) {
		/* This is copied from Ilia's patch */
		memcpy(&len, payload, sizeof(uint32_t));
		buffer = emalloc(len + 1);
		*payload_len -= sizeof(uint32_t);
		payload += sizeof(uint32_t);
		length = len;

		if (MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSION_FASTLZ)) {
			decompress_status = ((length = fastlz_decompress(payload, *payload_len, buffer, len)) > 0);
		} else if (MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSION_ZLIB)) {
			decompress_status = (uncompress((Bytef *)buffer, &length, (Bytef *)payload, *payload_len) == Z_OK);
		}
	}

	/* Fall back to 'old style decompression' */
	if (!decompress_status) {
		unsigned int factor = 1, maxfactor = 16;
		int status;

		do {
			length = (unsigned long)*payload_len * (1 << factor++);
			buffer = erealloc(buffer, length + 1);
			memset(buffer, 0, length + 1);
			status = uncompress((Bytef *)buffer, (uLongf *)&length, (const Bytef *)payload, *payload_len);
		} while ((status==Z_BUF_ERROR) && (factor < maxfactor));

		if (status == Z_OK) {
			decompress_status = 1;
		}
	}

	if (!decompress_status) {
		php_error_docref(NULL, E_WARNING, "could not decompress value");
		efree(buffer);
		return NULL;
	}
	buffer [length] = '\0';
	*payload_len    = length;
	return buffer;
}

static
zend_bool s_unserialize_value (enum memcached_serializer serializer, int val_type, zval *value, const char *payload, size_t payload_len)
{
	switch (val_type) {
		case MEMC_VAL_IS_SERIALIZED:
		{
			const char *payload_tmp = payload;
			php_unserialize_data_t var_hash;

			PHP_VAR_UNSERIALIZE_INIT(var_hash);
			if (!php_var_unserialize(value, (const unsigned char **)&payload_tmp, (const unsigned char *)payload_tmp + payload_len, &var_hash)) {
				zval_ptr_dtor(value);
				ZVAL_FALSE(value);
				PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
				php_error_docref(NULL, E_WARNING, "could not unserialize value");
				return 0;
			}
			PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
		}
			break;

		case MEMC_VAL_IS_IGBINARY:
#ifdef HAVE_MEMCACHED_IGBINARY
			if (igbinary_unserialize((uint8_t *)payload, payload_len, &value)) {
				ZVAL_FALSE(value);
				php_error_docref(NULL, E_WARNING, "could not unserialize value with igbinary");
				return 0;
			}
#else
			ZVAL_FALSE(value);
			php_error_docref(NULL, E_WARNING, "could not unserialize value, no igbinary support");
			return 0;
#endif
			break;

		case MEMC_VAL_IS_JSON:
#ifdef HAVE_JSON_API
			php_json_decode(value, (char*) payload, payload_len, (serializer == SERIALIZER_JSON_ARRAY), PHP_JSON_PARSER_DEFAULT_DEPTH);
#else
			ZVAL_FALSE(value);
			php_error_docref(NULL, E_WARNING, "could not unserialize value, no json support");
			return 0;
#endif
			break;

        case MEMC_VAL_IS_MSGPACK:
#ifdef HAVE_MEMCACHED_MSGPACK
			php_msgpack_unserialize(value, payload, payload_len);
#else
			ZVAL_FALSE(value);
			php_error_docref(NULL, E_WARNING, "could not unserialize value, no msgpack support");
			return 0;
#endif
 			break;
	}
	return 1;
}

/* The caller MUST free the payload */
static int php_memc_zval_from_payload(zval *value, const char *payload_in, size_t payload_len, uint32_t flags, enum memcached_serializer serializer)
{
	/*
	   A NULL payload is completely valid if length is 0, it is simply empty.
	 */
	int retval = 0;
	zend_bool payload_emalloc = 0;
	char *pl = NULL;

	if (payload_in == NULL && payload_len > 0) {
		ZVAL_FALSE(value);
		php_error_docref(NULL, E_WARNING,
			"Could not handle non-existing value of length %zu", payload_len);
		return -1;
	} else if (payload_in == NULL) {
		if (MEMC_VAL_GET_TYPE(flags) == MEMC_VAL_IS_BOOL) {
			ZVAL_FALSE(value);
		} else {
			ZVAL_EMPTY_STRING(value);
		}
		return 0;
	}

	if (MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSED)) {
		char *datas = s_decompress_value (payload_in, &payload_len, flags);
		if (!datas) {
			ZVAL_FALSE(value);
			return -1;
		}
		pl = datas;
		payload_emalloc = 1;
	} else {
		pl = (char *) payload_in;
	}

	switch (MEMC_VAL_GET_TYPE(flags)) {
		case MEMC_VAL_IS_STRING:
			ZVAL_STRINGL(value, pl, payload_len);
			break;

		case MEMC_VAL_IS_LONG:
		{
			long lval;
			char conv_buf [128];

			if (payload_len >= 128) {
				php_error_docref(NULL, E_WARNING, "could not read long value, too big");
				retval = -1;
			}
			else {
				memcpy (conv_buf, pl, payload_len);
				conv_buf [payload_len] = '\0';

				lval = strtol(conv_buf, NULL, 10);
				ZVAL_LONG(value, lval);
			}
		}
			break;

		case MEMC_VAL_IS_DOUBLE:
		{
			char conv_buf [128];

			if (payload_len >= 128) {
				php_error_docref(NULL, E_WARNING, "could not read double value, too big");
				retval = -1;
			}
			else {
				memcpy (conv_buf, pl, payload_len);
				conv_buf [payload_len] = '\0';

				if (payload_len == 8 && memcmp(conv_buf, "Infinity", 8) == 0) {
					ZVAL_DOUBLE(value, php_get_inf());
				} else if (payload_len == 9 && memcmp(conv_buf, "-Infinity", 9) == 0) {
					ZVAL_DOUBLE(value, -php_get_inf());
				} else if (payload_len == 3 && memcmp(conv_buf, "NaN", 3) == 0) {
					ZVAL_DOUBLE(value, php_get_nan());
				} else {
					ZVAL_DOUBLE(value, zend_strtod(conv_buf, NULL));
				}
			}
		}
			break;

		case MEMC_VAL_IS_BOOL:
			ZVAL_BOOL(value, payload_len > 0 && pl[0] == '1');
			break;

		case MEMC_VAL_IS_SERIALIZED:
		case MEMC_VAL_IS_IGBINARY:
		case MEMC_VAL_IS_JSON:
		case MEMC_VAL_IS_MSGPACK:
			if (!s_unserialize_value (serializer, MEMC_VAL_GET_TYPE(flags), value, pl, payload_len)) {
				retval = -1;
			}
			break;

		default:
			ZVAL_FALSE(value);
			php_error_docref(NULL, E_WARNING, "unknown payload type");
			retval = -1;
			break;
	}

	if (payload_emalloc) {
		efree(pl);
	}
	return retval;
}

PHP_MEMCACHED_API
zend_class_entry *php_memc_get_ce(void)
{
	return memcached_ce;
}

PHP_MEMCACHED_API
zend_class_entry *php_memc_get_exception(void)
{
	return memcached_exception_ce;
}

PHP_MEMCACHED_API
zend_class_entry *php_memc_get_exception_base(int root)
{
#if HAVE_SPL
	if (!root) {
		if (!spl_ce_RuntimeException) {
			zend_class_entry *pce;
			zval *pce_z;

			if ((pce_z = zend_hash_str_find(CG(class_table),
											"runtimeexception",
											sizeof("RuntimeException") - 1)) != NULL) {
				pce = Z_CE_P(pce_z);
				spl_ce_RuntimeException = pce;
				return pce;
			}
		} else {
			return spl_ce_RuntimeException;
		}
	}
#endif
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default();
#endif
}

static memcached_return php_memc_do_cache_callback(zval *zmemc_obj, zend_fcall_info *fci,
	zend_fcall_info_cache *fcc, zend_string *key, zval *value)
{
	char *payload = NULL;
	size_t payload_len = 0;
	zval params[4];
	zval retval;
	zval z_key;
	zval z_val;
	zval *expiration, z_expiration;

	uint32_t flags = 0;
	memcached_return rc;
	php_memc_t* i_obj;
	memcached_return status = MEMCACHED_SUCCESS;
	int result;

	ZVAL_STR(&z_key, key);
	ZVAL_NULL(&z_val);
	ZVAL_NEW_REF(&z_val, value);
	ZVAL_NEW_REF(&z_expiration, value);
	ZVAL_LONG(Z_REFVAL(z_expiration), 0);

	ZVAL_COPY(&params[0], zmemc_obj);
	ZVAL_COPY(&params[1], &z_key);
	ZVAL_COPY_VALUE(&params[2], &z_val);
	ZVAL_COPY_VALUE(&params[3], &z_expiration);

	fci->retval = &retval;
	fci->params = params;
	fci->param_count = 4;

	result = zend_call_function(fci, fcc);
	
	ZVAL_DUP(value, Z_REFVAL(z_val));
	expiration = Z_REFVAL(z_expiration);
	if (result == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {
		struct memc_obj *m_obj;
		i_obj = Z_MEMC_OBJ_P(zmemc_obj)
		m_obj = i_obj->obj;

		if (zend_is_true(&retval)) {
			time_t expir;

			expir = zval_get_long(expiration);

			payload = php_memc_zval_to_payload(value, &payload_len, &flags, m_obj->serializer, m_obj->compression_type);
			if (payload == NULL) {
				status = (memcached_return)MEMC_RES_PAYLOAD_FAILURE;
			} else {
				rc = memcached_set(m_obj->memc, key->val, key->len, payload, payload_len, expir, flags);
				if (rc == MEMCACHED_SUCCESS || rc == MEMCACHED_BUFFERED) {
					status = rc;
				}
				efree(payload);
			}
		} else {
			status = MEMCACHED_NOTFOUND;
			zval_dtor(value);
			ZVAL_NULL(value);
		}

	} else {
		if (result == FAILURE) {
			php_error_docref(NULL, E_WARNING, "could not invoke cache callback");
		}
		status = MEMCACHED_FAILURE;
		zval_dtor(value);
		ZVAL_NULL(value);
	}

	if (&retval) {
		zval_ptr_dtor(&retval);
	}

	zval_ptr_dtor(&z_key);
	zval_ptr_dtor(&z_val);
	zval_ptr_dtor(&z_expiration);
	zval_ptr_dtor(zmemc_obj);

	return status;
}

static int php_memc_do_result_callback(zval *zmemc_obj, zend_fcall_info *fci,
	zend_fcall_info_cache *fcc,
	memcached_result_st *result)
{
	const char *res_key = NULL;
	size_t res_key_len = 0;
	const char  *payload = NULL;
	size_t payload_len = 0;
	zval value;
	zval retval;
	uint64_t cas = 0;
	zval z_result;
	uint32_t flags = 0;
	int rc = 0;
	php_memc_t *i_obj = NULL;

	fci->retval = &retval;
	fci->param_count = 2;

	payload     = memcached_result_value(result);
	payload_len = memcached_result_length(result);
	flags       = memcached_result_flags(result);
	res_key     = memcached_result_key_value(result);
	res_key_len = memcached_result_key_length(result);
	cas 		= memcached_result_cas(result);

	i_obj = Z_MEMC_OBJ_P(zmemc_obj);

	if (php_memc_zval_from_payload(&value, payload, payload_len, flags, i_obj->obj->serializer) < 0) {
		zval_ptr_dtor(&value);
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		return -1;
	}

	array_init(&z_result);
	add_assoc_stringl_ex(&z_result, ZEND_STRL("key"), (char *)res_key, res_key_len);
	add_assoc_zval_ex(&z_result, ZEND_STRL("value"), &value);
	if (cas != 0) {
		add_assoc_double_ex(&z_result, ZEND_STRL("cas"), (double)cas);
	}
	if (MEMC_VAL_GET_USER_FLAGS(flags) != 0) {
		add_assoc_long_ex(&z_result, ZEND_STRL("flags"), MEMC_VAL_GET_USER_FLAGS(flags));
	}

	ZVAL_UNDEF(&retval);
	zend_fcall_info_argn(fci, 2, zmemc_obj, &z_result);

	if (zend_call_function(fci, fcc) == FAILURE) {
		php_error_docref(NULL, E_WARNING, "could not invoke result callback");
		rc = -1;
	}

	if (Z_TYPE(retval) != IS_UNDEF) {
		zval_ptr_dtor(&retval);
	}

	zend_fcall_info_args_clear(fci, 1);
	zval_ptr_dtor(&z_result);

	return rc;
}
/* }}} */

#ifdef HAVE_MEMCACHED_PROTOCOL

static
void s_destroy_cb (zend_fcall_info *fci)
{
	if (fci->size > 0) {
		zval_ptr_dtor(&fci->function_name);
		if (fci->object_ptr != NULL) {
			zval_ptr_dtor(&fci->object_ptr);
		}
	}
}

static
PHP_METHOD(MemcachedServer, run)
{
	int i;
	zend_bool rc;
	zend *address;

	php_memc_server_t *intern;
	intern = Z_MEMC_OBJ_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &address) == FAILURE) {
		return;
	}

	rc = php_memc_proto_handler_run (intern->handler, address);

	for (i = MEMC_SERVER_ON_MIN + 1; i < MEMC_SERVER_ON_MAX; i++) {
		s_destroy_cb (&MEMC_G(server.callbacks) [i].fci);
	}

	RETURN_BOOL(rc);
}

static
PHP_METHOD(MemcachedServer, on)
{
	long event;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
	zend_bool rc = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lf!", &event, &fci, &fci_cache) == FAILURE) {
		return;
	}

	if (event <= MEMC_SERVER_ON_MIN || event >= MEMC_SERVER_ON_MAX) {
		RETURN_FALSE;
	}

	if (fci.size > 0) {
		s_destroy_cb (&MEMC_G(server.callbacks) [event].fci);

		MEMC_G(server.callbacks) [event].fci       = fci;
		MEMC_G(server.callbacks) [event].fci_cache = fci_cache;

		Z_ADDREF_P (fci.function_name);
		if (fci.object_ptr) {
			Z_ADDREF_P (fci.object_ptr);
		}
	}
	RETURN_BOOL(rc);
}

#endif

/* {{{ methods arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, persistent_id)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getResultCode, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getResultMessage, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, cache_cb)
	ZEND_ARG_INFO(2, cas_token)
	ZEND_ARG_INFO(1, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, cache_cb)
	ZEND_ARG_INFO(2, cas_token)
	ZEND_ARG_INFO(1, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getMulti, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(2, cas_tokens)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_INFO(1, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(2, cas_tokens)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_INFO(1, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getDelayed, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(0, with_cas)
	ZEND_ARG_INFO(0, value_cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getDelayedByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(0, with_cas)
	ZEND_ARG_INFO(0, value_cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fetch, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fetchAll, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_touch, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_touchByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setMulti, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, items, 0)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_ARRAY_INFO(0, items, 0)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_add, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_addByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_replace, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_replaceByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_append, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appendByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_prepend, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_prependByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_cas, 0, 0, 3)
	ZEND_ARG_INFO(0, cas_token)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_casByKey, 0, 0, 4)
	ZEND_ARG_INFO(0, cas_token)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
	ZEND_ARG_INFO(0, udf_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_deleteMulti, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_deleteByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_deleteMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_increment, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, initial_value)
	ZEND_ARG_INFO(0, expiry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_decrement, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, initial_value)
	ZEND_ARG_INFO(0, expiry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_incrementByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, initial_value)
	ZEND_ARG_INFO(0, expiry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_decrementByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, initial_value)
	ZEND_ARG_INFO(0, expiry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_flush, 0, 0, 0)
	ZEND_ARG_INFO(0, delay)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_addServer, 0, 0, 2)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, port)
	ZEND_ARG_INFO(0, weight)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_addServers, 0)
	ZEND_ARG_ARRAY_INFO(0, servers, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getServerList, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_resetServerList, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_quit, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_flushBuffers, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getServerByKey, 0)
	ZEND_ARG_INFO(0, server_key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getLastErrorMessage, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getLastErrorCode, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getLastErrorErrno, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getLastDisconnectedServer, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getOption, 0)
	ZEND_ARG_INFO(0, option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_setSaslAuthData, 0)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_setOption, 0)
	ZEND_ARG_INFO(0, option)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_setOptions, 0)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_setBucket, 3)
	ZEND_ARG_INFO(0, host_map)
	ZEND_ARG_INFO(0, forward_map)
	ZEND_ARG_INFO(0, replicas)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getStats, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getVersion, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_isPersistent, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_isPristine, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getAllKeys, 0)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ memcached_class_methods */
#define MEMC_ME(name, args) PHP_ME(Memcached, name, args, ZEND_ACC_PUBLIC)
static zend_function_entry memcached_class_methods[] = {
	MEMC_ME(__construct,        arginfo___construct)

	MEMC_ME(getResultCode,      arginfo_getResultCode)
	MEMC_ME(getResultMessage,   arginfo_getResultMessage)

	MEMC_ME(get,                arginfo_get)
	MEMC_ME(getByKey,           arginfo_getByKey)
	MEMC_ME(getMulti,           arginfo_getMulti)
	MEMC_ME(getMultiByKey,      arginfo_getMultiByKey)
	MEMC_ME(getDelayed,         arginfo_getDelayed)
	MEMC_ME(getDelayedByKey,    arginfo_getDelayedByKey)
	MEMC_ME(fetch,              arginfo_fetch)
	MEMC_ME(fetchAll,           arginfo_fetchAll)

	MEMC_ME(set,                arginfo_set)
	MEMC_ME(setByKey,           arginfo_setByKey)
#ifdef HAVE_MEMCACHED_TOUCH
	MEMC_ME(touch,              arginfo_touch)
	MEMC_ME(touchByKey,         arginfo_touchByKey)
#endif
	MEMC_ME(setMulti,           arginfo_setMulti)
	MEMC_ME(setMultiByKey,      arginfo_setMultiByKey)

	MEMC_ME(cas,                arginfo_cas)
	MEMC_ME(casByKey,           arginfo_casByKey)
	MEMC_ME(add,                arginfo_add)
	MEMC_ME(addByKey,           arginfo_addByKey)
	MEMC_ME(append,             arginfo_append)
	MEMC_ME(appendByKey,        arginfo_appendByKey)
	MEMC_ME(prepend,            arginfo_prepend)
	MEMC_ME(prependByKey,       arginfo_prependByKey)
	MEMC_ME(replace,            arginfo_replace)
	MEMC_ME(replaceByKey,       arginfo_replaceByKey)
	MEMC_ME(delete,             arginfo_delete)
	MEMC_ME(deleteMulti,        arginfo_deleteMulti)
	MEMC_ME(deleteByKey,        arginfo_deleteByKey)
	MEMC_ME(deleteMultiByKey,   arginfo_deleteMultiByKey)

	MEMC_ME(increment,          arginfo_increment)
	MEMC_ME(decrement,          arginfo_decrement)
	MEMC_ME(incrementByKey,     arginfo_incrementByKey)
	MEMC_ME(decrementByKey,     arginfo_decrementByKey)

	MEMC_ME(addServer,          arginfo_addServer)
	MEMC_ME(addServers,         arginfo_addServers)
	MEMC_ME(getServerList,      arginfo_getServerList)
	MEMC_ME(getServerByKey,     arginfo_getServerByKey)
	MEMC_ME(resetServerList,    arginfo_resetServerList)
	MEMC_ME(quit,               arginfo_quit)
	MEMC_ME(flushBuffers,       arginfo_flushBuffers)

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00049000
	MEMC_ME(getLastErrorMessage,		arginfo_getLastErrorMessage)
	MEMC_ME(getLastErrorCode,		arginfo_getLastErrorCode)
	MEMC_ME(getLastErrorErrno,		arginfo_getLastErrorErrno)
#endif
	MEMC_ME(getLastDisconnectedServer,	arginfo_getLastDisconnectedServer)

	MEMC_ME(getStats,           arginfo_getStats)
	MEMC_ME(getVersion,         arginfo_getVersion)
	MEMC_ME(getAllKeys,         arginfo_getAllKeys)

	MEMC_ME(flush,              arginfo_flush)

	MEMC_ME(getOption,          arginfo_getOption)
	MEMC_ME(setOption,          arginfo_setOption)
	MEMC_ME(setOptions,         arginfo_setOptions)
	MEMC_ME(setBucket,          arginfo_setBucket)
#ifdef HAVE_MEMCACHED_SASL
    MEMC_ME(setSaslAuthData,    arginfo_setSaslAuthData)
#endif
	MEMC_ME(isPersistent,       arginfo_isPersistent)
	MEMC_ME(isPristine,         arginfo_isPristine)
	{ NULL, NULL, NULL }
};
#undef MEMC_ME
/* }}} */

#ifdef HAVE_MEMCACHED_PROTOCOL
/* {{{ */
#define MEMC_SE_ME(name, args) PHP_ME(MemcachedServer, name, args, ZEND_ACC_PUBLIC)
static
zend_function_entry memcached_server_class_methods[] = {
	MEMC_SE_ME(run, NULL)
	MEMC_SE_ME(on, NULL)
	{ NULL, NULL, NULL }
};
#undef MEMC_SE_ME
/* }}} */
#endif

/* {{{ memcached_module_entry
 */

#if ZEND_MODULE_API_NO >= 20050922
static const zend_module_dep memcached_deps[] = {
#ifdef HAVE_MEMCACHED_SESSION
	ZEND_MOD_REQUIRED("session")
#endif
#ifdef HAVE_MEMCACHED_IGBINARY
	ZEND_MOD_REQUIRED("igbinary")
#endif
#ifdef HAVE_MEMCACHED_MSGPACK
	ZEND_MOD_REQUIRED("msgpack")
#endif
#ifdef HAVE_SPL
	ZEND_MOD_REQUIRED("spl")
#endif
	{NULL, NULL, NULL}
};
#endif

static
PHP_GINIT_FUNCTION(php_memcached)
{
#ifdef HAVE_MEMCACHED_SESSION
	php_memcached_globals->sess_locking_enabled = 1;
	php_memcached_globals->sess_binary_enabled = 1;
	php_memcached_globals->sess_consistent_hash_enabled = 0;
	php_memcached_globals->sess_number_of_replicas = 0;
	php_memcached_globals->sess_remove_failed_enabled = 0;
	php_memcached_globals->sess_prefix = NULL;
	php_memcached_globals->sess_lock_wait = 0;
	php_memcached_globals->sess_lock_max_wait = 0;
	php_memcached_globals->sess_lock_expire = 0;
	php_memcached_globals->sess_locked = 0;
	php_memcached_globals->sess_lock_key = NULL;
	php_memcached_globals->sess_lock_key_len = 0;
	php_memcached_globals->sess_randomize_replica_read = 0;
	php_memcached_globals->sess_connect_timeout = 1000;
#if HAVE_MEMCACHED_SASL
	php_memcached_globals->sess_sasl_username = NULL;
	php_memcached_globals->sess_sasl_password = NULL;
	php_memcached_globals->sess_sasl_data = 0;
#endif
#endif
	php_memcached_globals->serializer_name = NULL;
	php_memcached_globals->serializer = SERIALIZER_DEFAULT;
	php_memcached_globals->compression_type = NULL;
	php_memcached_globals->compression_type_real = COMPRESSION_TYPE_FASTLZ;
	php_memcached_globals->compression_factor = 1.30;
#if HAVE_MEMCACHED_SASL
	php_memcached_globals->use_sasl = 0;
#endif
	php_memcached_globals->store_retry_count = 2;
}

zend_module_entry memcached_module_entry = {
	STANDARD_MODULE_HEADER,
	"memcached",
	NULL,
	PHP_MINIT(memcached),
	PHP_MSHUTDOWN(memcached),
	NULL,
	NULL,
	PHP_MINFO(memcached),
	PHP_MEMCACHED_VERSION,
	PHP_MODULE_GLOBALS(php_memcached),
	PHP_GINIT(php_memcached),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/* {{{ php_memc_register_constants */
static void php_memc_register_constants(INIT_FUNC_ARGS)
{
	#define REGISTER_MEMC_CLASS_CONST_LONG(name, value) zend_declare_class_constant_long(php_memc_get_ce() , ZEND_STRS( #name ) - 1, value)
	#define REGISTER_MEMC_CLASS_CONST_BOOL(name, value) zend_declare_class_constant_bool(php_memc_get_ce() , ZEND_STRS( #name ) - 1, value)
	#define REGISTER_MEMC_CLASS_CONST_NULL(name) zend_declare_class_constant_null(php_memc_get_ce() , ZEND_STRS( #name ) - 1)

	/*
	 * Class options
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(LIBMEMCACHED_VERSION_HEX, LIBMEMCACHED_VERSION_HEX);

	REGISTER_MEMC_CLASS_CONST_LONG(OPT_COMPRESSION, MEMC_OPT_COMPRESSION);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_COMPRESSION_TYPE, MEMC_OPT_COMPRESSION_TYPE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_PREFIX_KEY,  MEMC_OPT_PREFIX_KEY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERIALIZER,  MEMC_OPT_SERIALIZER);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_STORE_RETRY_COUNT,  MEMC_OPT_STORE_RETRY_COUNT);

	/*
	 * Indicate whether igbinary serializer is available
	 */
#ifdef HAVE_MEMCACHED_IGBINARY
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_IGBINARY, 1);
#else
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_IGBINARY, 0);
#endif

	/*
	 * Indicate whether json serializer is available
	 */
#ifdef HAVE_JSON_API
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_JSON, 1);
#else
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_JSON, 0);
#endif

    /*
     * Indicate whether msgpack serializer is available
     */
#ifdef HAVE_MEMCACHED_MSGPACK
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_MSGPACK, 1);
#else
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_MSGPACK, 0);
#endif

#ifdef HAVE_MEMCACHED_SESSION
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_SESSION, 1);
#else
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_SESSION, 0);
#endif

#ifdef HAVE_MEMCACHED_SASL
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_SASL, 1);
#else
	REGISTER_MEMC_CLASS_CONST_LONG(HAVE_SASL, 0);
#endif

	/*
	 * libmemcached behavior options
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(OPT_HASH, MEMCACHED_BEHAVIOR_HASH);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_DEFAULT, MEMCACHED_HASH_DEFAULT);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_MD5, MEMCACHED_HASH_MD5);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_CRC, MEMCACHED_HASH_CRC);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1_64, MEMCACHED_HASH_FNV1_64);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1A_64, MEMCACHED_HASH_FNV1A_64);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1_32, MEMCACHED_HASH_FNV1_32);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1A_32, MEMCACHED_HASH_FNV1A_32);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_HSIEH, MEMCACHED_HASH_HSIEH);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_MURMUR, MEMCACHED_HASH_MURMUR);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_DISTRIBUTION, MEMCACHED_BEHAVIOR_DISTRIBUTION);
	REGISTER_MEMC_CLASS_CONST_LONG(DISTRIBUTION_MODULA, MEMCACHED_DISTRIBUTION_MODULA);
	REGISTER_MEMC_CLASS_CONST_LONG(DISTRIBUTION_CONSISTENT, MEMCACHED_DISTRIBUTION_CONSISTENT);
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00049000
	REGISTER_MEMC_CLASS_CONST_LONG(DISTRIBUTION_VIRTUAL_BUCKET, MEMCACHED_DISTRIBUTION_VIRTUAL_BUCKET);
#endif
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_LIBKETAMA_COMPATIBLE, MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_LIBKETAMA_HASH, MEMCACHED_BEHAVIOR_KETAMA_HASH);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_TCP_KEEPALIVE, MEMCACHED_BEHAVIOR_TCP_KEEPALIVE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_BUFFER_WRITES, MEMCACHED_BEHAVIOR_BUFFER_REQUESTS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_BINARY_PROTOCOL, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_NO_BLOCK, MEMCACHED_BEHAVIOR_NO_BLOCK);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_TCP_NODELAY, MEMCACHED_BEHAVIOR_TCP_NODELAY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SOCKET_SEND_SIZE, MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SOCKET_RECV_SIZE, MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_CONNECT_TIMEOUT, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RETRY_TIMEOUT, MEMCACHED_BEHAVIOR_RETRY_TIMEOUT);
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000003
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_DEAD_TIMEOUT, MEMCACHED_BEHAVIOR_DEAD_TIMEOUT);
#endif
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SEND_TIMEOUT, MEMCACHED_BEHAVIOR_SND_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RECV_TIMEOUT, MEMCACHED_BEHAVIOR_RCV_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_POLL_TIMEOUT, MEMCACHED_BEHAVIOR_POLL_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_CACHE_LOOKUPS, MEMCACHED_BEHAVIOR_CACHE_LOOKUPS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERVER_FAILURE_LIMIT, MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_AUTO_EJECT_HOSTS, MEMCACHED_BEHAVIOR_AUTO_EJECT_HOSTS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_HASH_WITH_PREFIX_KEY, MEMCACHED_BEHAVIOR_HASH_WITH_PREFIX_KEY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_NOREPLY, MEMCACHED_BEHAVIOR_NOREPLY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SORT_HOSTS, MEMCACHED_BEHAVIOR_SORT_HOSTS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_VERIFY_KEY, MEMCACHED_BEHAVIOR_VERIFY_KEY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_USE_UDP, MEMCACHED_BEHAVIOR_USE_UDP);
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x00037000
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_NUMBER_OF_REPLICAS, MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RANDOMIZE_REPLICA_READ, MEMCACHED_BEHAVIOR_RANDOMIZE_REPLICA_READ);
#endif
#ifdef HAVE_MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_REMOVE_FAILED_SERVERS, MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS);
#endif
#ifdef HAVE_MEMCACHED_BEHAVIOR_SERVER_TIMEOUT_LIMIT
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERVER_TIMEOUT_LIMIT, MEMCACHED_BEHAVIOR_SERVER_TIMEOUT_LIMIT);
#endif

	/*
	 * libmemcached result codes
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(RES_SUCCESS,              MEMCACHED_SUCCESS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_FAILURE,              MEMCACHED_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_HOST_LOOKUP_FAILURE,  MEMCACHED_HOST_LOOKUP_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_UNKNOWN_READ_FAILURE, MEMCACHED_UNKNOWN_READ_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_PROTOCOL_ERROR,       MEMCACHED_PROTOCOL_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_CLIENT_ERROR,         MEMCACHED_CLIENT_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_ERROR,         MEMCACHED_SERVER_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_WRITE_FAILURE,        MEMCACHED_WRITE_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_DATA_EXISTS,          MEMCACHED_DATA_EXISTS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NOTSTORED,            MEMCACHED_NOTSTORED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NOTFOUND,             MEMCACHED_NOTFOUND);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_PARTIAL_READ,         MEMCACHED_PARTIAL_READ);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SOME_ERRORS,          MEMCACHED_SOME_ERRORS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NO_SERVERS,           MEMCACHED_NO_SERVERS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_END,                  MEMCACHED_END);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_ERRNO,                MEMCACHED_ERRNO);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_BUFFERED,             MEMCACHED_BUFFERED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_TIMEOUT,              MEMCACHED_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_BAD_KEY_PROVIDED,     MEMCACHED_BAD_KEY_PROVIDED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_STORED,               MEMCACHED_STORED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_DELETED,              MEMCACHED_DELETED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_STAT,                 MEMCACHED_STAT);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_ITEM,                 MEMCACHED_ITEM);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NOT_SUPPORTED,        MEMCACHED_NOT_SUPPORTED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_FETCH_NOTFINISHED,    MEMCACHED_FETCH_NOTFINISHED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_MARKED_DEAD,   MEMCACHED_SERVER_MARKED_DEAD);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_UNKNOWN_STAT_KEY,     MEMCACHED_UNKNOWN_STAT_KEY);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_INVALID_HOST_PROTOCOL, MEMCACHED_INVALID_HOST_PROTOCOL);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_MEMORY_ALLOCATION_FAILURE, MEMCACHED_MEMORY_ALLOCATION_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_CONNECTION_SOCKET_CREATE_FAILURE, MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE);

	REGISTER_MEMC_CLASS_CONST_LONG(RES_E2BIG,                            MEMCACHED_E2BIG);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_KEY_TOO_BIG,                      MEMCACHED_KEY_TOO_BIG);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_TEMPORARILY_DISABLED,      MEMCACHED_SERVER_TEMPORARILY_DISABLED);

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000008
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_MEMORY_ALLOCATION_FAILURE, MEMCACHED_SERVER_MEMORY_ALLOCATION_FAILURE);
#endif

#if HAVE_MEMCACHED_SASL
	REGISTER_MEMC_CLASS_CONST_LONG(RES_AUTH_PROBLEM,  MEMCACHED_AUTH_PROBLEM);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_AUTH_FAILURE,  MEMCACHED_AUTH_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_AUTH_CONTINUE, MEMCACHED_AUTH_CONTINUE);
#endif

	/*
	 * Our result codes.
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(RES_PAYLOAD_FAILURE, MEMC_RES_PAYLOAD_FAILURE);

	/*
	 * Serializer types.
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_PHP, SERIALIZER_PHP);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_IGBINARY, SERIALIZER_IGBINARY);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_JSON, SERIALIZER_JSON);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_JSON_ARRAY, SERIALIZER_JSON_ARRAY);
    REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_MSGPACK, SERIALIZER_MSGPACK);

	/*
	 * Compression types
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(COMPRESSION_FASTLZ, COMPRESSION_TYPE_FASTLZ);
	REGISTER_MEMC_CLASS_CONST_LONG(COMPRESSION_ZLIB,   COMPRESSION_TYPE_ZLIB);

	/*
	 * Flags.
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(GET_PRESERVE_ORDER, MEMC_GET_PRESERVE_ORDER);

#ifdef HAVE_MEMCACHED_PROTOCOL
	/*
	 * Server callbacks
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(ON_CONNECT,   MEMC_SERVER_ON_CONNECT);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_ADD,       MEMC_SERVER_ON_ADD);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_APPEND,    MEMC_SERVER_ON_APPEND);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_DECREMENT, MEMC_SERVER_ON_DECREMENT);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_DELETE,    MEMC_SERVER_ON_DELETE);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_FLUSH,     MEMC_SERVER_ON_FLUSH);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_GET,       MEMC_SERVER_ON_GET);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_INCREMENT, MEMC_SERVER_ON_INCREMENT);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_NOOP,      MEMC_SERVER_ON_NOOP);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_PREPEND,   MEMC_SERVER_ON_PREPEND);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_QUIT,      MEMC_SERVER_ON_QUIT);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_REPLACE,   MEMC_SERVER_ON_REPLACE);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_SET,       MEMC_SERVER_ON_SET);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_STAT,      MEMC_SERVER_ON_STAT);
	REGISTER_MEMC_CLASS_CONST_LONG(ON_VERSION,   MEMC_SERVER_ON_VERSION);

	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_SUCCESS,         PROTOCOL_BINARY_RESPONSE_SUCCESS);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_KEY_ENOENT,      PROTOCOL_BINARY_RESPONSE_KEY_ENOENT);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_KEY_EEXISTS,     PROTOCOL_BINARY_RESPONSE_KEY_EEXISTS);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_E2BIG,           PROTOCOL_BINARY_RESPONSE_E2BIG);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_EINVAL,          PROTOCOL_BINARY_RESPONSE_EINVAL);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_NOT_STORED,      PROTOCOL_BINARY_RESPONSE_NOT_STORED);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_DELTA_BADVAL,    PROTOCOL_BINARY_RESPONSE_DELTA_BADVAL);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_NOT_MY_VBUCKET,  PROTOCOL_BINARY_RESPONSE_NOT_MY_VBUCKET);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_AUTH_ERROR,      PROTOCOL_BINARY_RESPONSE_AUTH_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_AUTH_CONTINUE,   PROTOCOL_BINARY_RESPONSE_AUTH_CONTINUE);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_UNKNOWN_COMMAND, PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_ENOMEM,          PROTOCOL_BINARY_RESPONSE_ENOMEM);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_NOT_SUPPORTED,   PROTOCOL_BINARY_RESPONSE_NOT_SUPPORTED);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_EINTERNAL,       PROTOCOL_BINARY_RESPONSE_EINTERNAL);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_EBUSY,           PROTOCOL_BINARY_RESPONSE_EBUSY);
	REGISTER_MEMC_CLASS_CONST_LONG(RESPONSE_ETMPFAIL,        PROTOCOL_BINARY_RESPONSE_ETMPFAIL);
#endif

	#undef REGISTER_MEMC_CLASS_CONST_LONG

	/*
	 * Return value from simple get errors
	 */
	REGISTER_MEMC_CLASS_CONST_BOOL(GET_ERROR_RETURN_VALUE, 0);
}
/* }}} */

int php_memc_sess_list_entry(void)
{
	return le_memc_sess;
}

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{
	zend_class_entry ce;

	memcpy(&memcached_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcached_object_handlers.offset = XtOffsetOf(php_memc_t, zo);
	memcached_object_handlers.clone_obj = NULL;
	memcached_object_handlers.free_obj = php_memc_free_storage;

	le_memc = zend_register_list_destructors_ex(NULL, php_memc_dtor, "Memcached persistent connection", module_number);
	le_memc_sess = zend_register_list_destructors_ex(NULL, php_memc_sess_dtor, "Memcached  Sessions persistent connection", module_number);

	INIT_CLASS_ENTRY(ce, "Memcached", memcached_class_methods);
	memcached_ce = zend_register_internal_class(&ce);
	memcached_ce->create_object = php_memc_new;

#ifdef HAVE_MEMCACHED_PROTOCOL
	memcpy(&memcached_server_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcached_server_object_handlers.clone_obj = NULL;

	INIT_CLASS_ENTRY(ce, "MemcachedServer", memcached_server_class_methods);
	memcached_server_ce = zend_register_internal_class(&ce);
	memcached_server_ce->create_object = php_memc_server_new;
#endif

	INIT_CLASS_ENTRY(ce, "MemcachedException", NULL);
	memcached_exception_ce = zend_register_internal_class_ex(&ce, php_memc_get_exception_base(0));
	/* TODO
	 * possibly declare custom exception property here
	 */

	php_memc_register_constants(INIT_FUNC_ARGS_PASSTHRU);

#ifdef HAVE_MEMCACHED_SESSION
	php_session_register_module(ps_memcached_ptr);
#endif

	REGISTER_INI_ENTRIES();
#if HAVE_MEMCACHED_SASL
	if (MEMC_G(use_sasl)) {
		if (sasl_client_init(NULL) != SASL_OK) {
			php_error_docref(NULL, E_ERROR, "Failed to initialize SASL library");
			return FAILURE;
		}
	}
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(memcached)
{
#if HAVE_MEMCACHED_SASL
	if (MEMC_G(use_sasl)) {
		sasl_done();
	}
#endif

	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(memcached)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "memcached support", "enabled");
	php_info_print_table_row(2, "Version", PHP_MEMCACHED_VERSION);
	php_info_print_table_row(2, "libmemcached version", memcached_lib_version());

#if HAVE_MEMCACHED_SASL
	php_info_print_table_row(2, "SASL support", "yes");
#else
	php_info_print_table_row(2, "SASL support", "no");
#endif

#ifdef HAVE_MEMCACHED_SESSION
	php_info_print_table_row(2, "Session support", "yes");
#else
	php_info_print_table_row(2, "Session support ", "no");
#endif

#ifdef HAVE_MEMCACHED_IGBINARY
	php_info_print_table_row(2, "igbinary support", "yes");
#else
	php_info_print_table_row(2, "igbinary support", "no");
#endif

#ifdef HAVE_JSON_API
	php_info_print_table_row(2, "json support", "yes");
#else
	php_info_print_table_row(2, "json support", "no");
#endif

#ifdef HAVE_MEMCACHED_MSGPACK
	php_info_print_table_row(2, "msgpack support", "yes");
#else
	php_info_print_table_row(2, "msgpack support", "no");
#endif

	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: noet sw=4 ts=4 fdm=marker:
 */
