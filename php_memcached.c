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

/* $ Id: $ */

/* TODO
 * - set LIBKETAMA_COMPATIBLE as the default?
 * - fix unserialize(serialize($memc))
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include "fastlz/fastlz.h"
#include <zlib.h>

/* Used to store the size of the block */
#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#ifdef PHP_WIN32
# include "win32/php_stdint.h"
#else
# ifndef HAVE_INT32_T
#  if SIZEOF_INT == 4
typedef int int32_t;
#  elif SIZEOF_LONG == 4
typedef long int int32_t;
#  endif
# endif

# ifndef HAVE_UINT32_T
#  if SIZEOF_INT == 4
typedef unsigned int uint32_t;
#  elif SIZEOF_LONG == 4
typedef unsigned long int uint32_t;
#  endif
# endif
#endif

#ifdef HAVE_JSON_API
# include "ext/json/php_json.h"
#endif

#ifdef HAVE_MEMCACHED_IGBINARY
# include "ext/igbinary/igbinary.h"
#endif

/*
 * This is needed because PHP 5.3.[01] does not install JSON_parser.h by default. This
 * constant will move into php_json.h in the future anyway.
 */
#ifndef JSON_PARSER_DEFAULT_DEPTH
#define JSON_PARSER_DEFAULT_DEPTH 512
#endif

/****************************************
  Custom options
****************************************/
#define MEMC_OPT_COMPRESSION   -1001
#define MEMC_OPT_PREFIX_KEY    -1002
#define MEMC_OPT_SERIALIZER    -1003

/****************************************
  Custom result codes
****************************************/
#define MEMC_RES_PAYLOAD_FAILURE -1001

/****************************************
  Payload value flags
****************************************/
#define MEMC_VAL_TYPE_MASK     0xf
#define MEMC_VAL_GET_TYPE(flags)         ((flags) & MEMC_VAL_TYPE_MASK)
#define MEMC_VAL_SET_TYPE(flags, type)   ((flags) |= ((type) & MEMC_VAL_TYPE_MASK))

#define MEMC_VAL_IS_STRING     0
#define MEMC_VAL_IS_LONG       1
#define MEMC_VAL_IS_DOUBLE     2
#define MEMC_VAL_IS_BOOL       3
#define MEMC_VAL_IS_SERIALIZED 4
#define MEMC_VAL_IS_IGBINARY   5
#define MEMC_VAL_IS_JSON       6

#define MEMC_VAL_COMPRESSED    (1<<4)
#define MEMC_VAL_COMPRESSION_ZLIB    (1<<5)
#define MEMC_VAL_COMPRESSION_FASTLZ  (1<<6)

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

#define MEMC_METHOD_FETCH_OBJECT                                               \
    i_obj = (php_memc_t *) zend_object_store_get_object( object TSRMLS_CC );   \
	m_obj = i_obj->obj; \
	if (!m_obj) {	\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Memcached constructor was not called");	\
		return;	\
	}

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

#define RETURN_FROM_GET RETURN_NULL()

/****************************************
  Structures and definitions
****************************************/
enum memcached_compression_type {
	COMPRESSION_TYPE_ZLIB = 1,
	COMPRESSION_TYPE_FASTLZ = 2,
};

typedef struct {
	zend_object zo;

	struct memc_obj {
		memcached_st *memc;
		bool compression;
		enum memcached_serializer serializer;
	} *obj;

	bool is_persistent;
	bool is_pristine;
	int rescode;
	int memc_errno;
} php_memc_t;

enum {
	MEMC_OP_SET,
	MEMC_OP_ADD,
	MEMC_OP_REPLACE,
	MEMC_OP_APPEND,
	MEMC_OP_PREPEND
};

static int le_memc;

static zend_class_entry *memcached_ce = NULL;
static zend_class_entry *memcached_exception_ce = NULL;
static zend_class_entry *spl_ce_RuntimeException = NULL;

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3)
const zend_fcall_info empty_fcall_info = { 0, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0 };
#undef ZEND_BEGIN_ARG_INFO_EX
#define ZEND_BEGIN_ARG_INFO_EX(name, pass_rest_by_reference, return_reference, required_num_args)   \
    static zend_arg_info name[] = {                                                                       \
        { NULL, 0, NULL, 0, 0, 0, pass_rest_by_reference, return_reference, required_num_args },
#endif

ZEND_DECLARE_MODULE_GLOBALS(php_memcached)

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif

static PHP_INI_MH(OnUpdateCompressionType)
{
	if (!new_value) {
		MEMC_G(compression_type_real) = COMPRESSION_TYPE_ZLIB;
	} else if (new_value_length == 6 && !strncmp(new_value, "fastlz", 6)) {
		MEMC_G(compression_type_real) = COMPRESSION_TYPE_FASTLZ;
	} else if (new_value_length == 4 && !strncmp(new_value, "zlib", 4)) {
		MEMC_G(compression_type_real) = COMPRESSION_TYPE_ZLIB;
	} else {
		return FAILURE;
	}
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

/* {{{ INI entries */
PHP_INI_BEGIN()
#if HAVE_MEMCACHED_SESSION
	STD_PHP_INI_ENTRY("memcached.sess_locking",		"1",		PHP_INI_ALL, OnUpdateBool,		sess_locking_enabled,	zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_lock_wait",		"150000",	PHP_INI_ALL, OnUpdateLongGEZero,sess_lock_wait,			zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_prefix",		"memc.sess.key.",	PHP_INI_ALL, OnUpdateString, sess_prefix,		zend_php_memcached_globals,	php_memcached_globals)
#endif
	STD_PHP_INI_ENTRY("memcached.compression_type",		"fastlz",	PHP_INI_ALL, OnUpdateCompressionType, compression_type,		zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.compression_factor",	"1.3",		PHP_INI_ALL, OnUpdateReal, compression_factor,		zend_php_memcached_globals,	php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.compression_threshold",	"2000",		PHP_INI_ALL, OnUpdateLong, compression_threshold,		zend_php_memcached_globals,	php_memcached_globals)

PHP_INI_END()
/* }}} */

/****************************************
  Forward declarations
****************************************/
static int php_memc_list_entry(void);
static int php_memc_handle_error(php_memc_t *i_obj, memcached_return status TSRMLS_DC);
static char *php_memc_zval_to_payload(zval *value, size_t *payload_len, uint32_t *flags, enum memcached_serializer serializer TSRMLS_DC);
static int php_memc_zval_from_payload(zval *value, char *payload, size_t payload_len, uint32_t flags, enum memcached_serializer serializer TSRMLS_DC);
static void php_memc_get_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_store_impl(INTERNAL_FUNCTION_PARAMETERS, int op, zend_bool by_key);
static void php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_cas_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_delete_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_incdec_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool incr);
static void php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static memcached_return php_memc_do_cache_callback(zval *memc_obj, zend_fcall_info *fci, zend_fcall_info_cache *fcc, char *key, size_t key_len, zval *value TSRMLS_DC);
static int php_memc_do_result_callback(zval *memc_obj, zend_fcall_info *fci, zend_fcall_info_cache *fcc, memcached_result_st *result TSRMLS_DC);


/****************************************
  Method implementations
****************************************/

/* {{{ Memcached::__construct([string persisten_id]))
   Creates a Memcached object, optionally using persistent memcache connection */
static PHP_METHOD(Memcached, __construct)
{
	zval *object = getThis();
	php_memc_t *i_obj;
	struct memc_obj *m_obj = NULL;
	char *persistent_id = NULL;
	int persistent_id_len;
	bool is_persistent = false;

	char *plist_key = NULL;
	int plist_key_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &persistent_id,
		&persistent_id_len) == FAILURE) {
		ZVAL_NULL(object);
		return;
	}

	i_obj = (php_memc_t *) zend_object_store_get_object(object TSRMLS_CC);
	i_obj->is_pristine = false;

	if (persistent_id) {
		zend_rsrc_list_entry *le = NULL;

		is_persistent = true;
		plist_key_len = spprintf(&plist_key, 0, "memcached:id=%s", persistent_id);
		plist_key_len += 1;

		if (plist_key == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of memory: cannot allocate peristent list handler");
			/* not reached */
		}

		if (zend_hash_find(&EG(persistent_list), plist_key, plist_key_len, (void *)&le) == SUCCESS) {
			if (le->type == php_memc_list_entry()) {
				m_obj = (struct memc_obj *) le->ptr;
			}
		}
	}

	if (!m_obj) {
		m_obj = pecalloc(1, sizeof(*m_obj), is_persistent);
		if (m_obj == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of memory: cannot allocate handle");
			/* not reached */
		}

		m_obj->memc = memcached_create(NULL);
		if (m_obj->memc == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not allocate libmemcached structure");
			/* not reached */
		}

		m_obj->serializer = MEMC_G(serializer);
		m_obj->compression = true;
		i_obj->is_pristine = true;

		if (is_persistent) {
			zend_rsrc_list_entry le;

			le.type = php_memc_list_entry();
			le.ptr = m_obj;
			if (zend_hash_update(&EG(persistent_list), (char *)plist_key,
				plist_key_len, (void *)&le, sizeof(le), NULL) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not register persistent entry");
				/* not reached */
			}
		}
	}

	i_obj->is_persistent = is_persistent;
	i_obj->obj = m_obj;

	if (plist_key != NULL) {
		efree(plist_key);
	}
}
/* }}} */

/* {{{ Memcached::get(string key [, mixed callback [, double &cas_token ] ])
   Returns a value for the given key or false */
PHP_METHOD(Memcached, get)
{
	php_memc_get_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getByKey(string server_key, string key [, mixed callback [, double &cas_token ] ])
   Returns a value for key from the server identified by the server key or false */
PHP_METHOD(Memcached, getByKey)
{
	php_memc_get_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_get_impl */
static void php_memc_get_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	char *key = NULL;
	size_t key_len = 0;
	char *server_key = NULL;
	int   server_key_len = 0;
	char  *payload = NULL;
	size_t payload_len = 0;
	uint32_t flags = 0;
	uint64_t cas = 0;
	const char* keys[1] = { NULL };
	size_t key_lens[1] = { 0 };
	zval *cas_token = NULL;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|f!z", &server_key,
								  &server_key_len, &key, &key_len, &fci, &fcc, &cas_token) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|f!z", &key, &key_len,
								  &fci, &fcc, &cas_token) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key_len == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FROM_GET;
	}

	keys[0] = key;
	key_lens[0] = key_len;

	if (cas_token) {

		uint64_t orig_cas_flag;

		/*
		 * Enable CAS support, but only if it is currently disabled.
		 */
		orig_cas_flag = memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS);
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);
		}

		status = memcached_mget_by_key(m_obj->memc, server_key, server_key_len, keys, key_lens, 1);

		if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
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
			if (status == MEMCACHED_NOTFOUND && fci.size != 0) {
				status = php_memc_do_cache_callback(getThis(), &fci, &fcc, key, key_len,
													return_value TSRMLS_CC);
				ZVAL_DOUBLE(cas_token, 0);
			}

			if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
				memcached_result_free(&result);
				RETURN_FROM_GET;
			}

			/* if we have a callback, all processing is done */
			if (fci.size != 0) {
				memcached_result_free(&result);
				return;
			}
		}

		payload     = memcached_result_value(&result);
		payload_len = memcached_result_length(&result);
		flags       = memcached_result_flags(&result);
		cas         = memcached_result_cas(&result);

		if (php_memc_zval_from_payload(return_value, payload, payload_len, flags, m_obj->serializer TSRMLS_CC) < 0) {
			memcached_result_free(&result);
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			RETURN_FROM_GET;
		}

		zval_dtor(cas_token);
		ZVAL_DOUBLE(cas_token, (double)cas);

		memcached_result_free(&result);

		/*
		 * Restore the CAS support flag, but only if we had to turn it on.
		 */
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
		}
		return;

	} else {
		int rc;
		bool return_value_set = false;

		status = memcached_mget_by_key(m_obj->memc, server_key, server_key_len, keys, key_lens, 1);
		payload = memcached_fetch(m_obj->memc, NULL, NULL, &payload_len, &flags, &status);

		/* This is for historical reasons */
		if (status == MEMCACHED_END)
			status = MEMCACHED_NOTFOUND;

		/*
		 * If payload wasn't found and we have a read-through callback, invoke it to get
		 * the value. The callback will take care of storing the value back into memcache.
		 * The callback will set the return value.
		 */
		if (payload == NULL && status == MEMCACHED_NOTFOUND && fci.size != 0) {
			size_t dummy_length;
			uint32_t dummy_flags;
			memcached_return dummy_status;

			status = php_memc_do_cache_callback(getThis(), &fci, &fcc, key, key_len,
												return_value TSRMLS_CC);
			return_value_set = true;

			(void)memcached_fetch(m_obj->memc, NULL, NULL, &dummy_length, &dummy_flags, &dummy_status);
		}

		if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
			if (payload) {
				free(payload);
			}
			RETURN_FROM_GET;
		}

		/* if memcached gave a value and there was no callback, payload may be NULL */
		if (!return_value_set) {
			rc = php_memc_zval_from_payload(return_value, payload, payload_len, flags, m_obj->serializer TSRMLS_CC);
			free(payload);
			if (rc < 0) {
				i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
				RETURN_FROM_GET;
			}
		}

	}
}
/* }}} */

/* {{{ Memcached::getMulti(array keys [, array &cas_tokens ])
   Returns values for the given keys or false */
PHP_METHOD(Memcached, getMulti)
{
	php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getMultiByKey(string server_key, array keys [, array &cas_tokens ])
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
	char *server_key = NULL;
	int   server_key_len = 0;
	size_t num_keys = 0;
	zval **entry = NULL;
	char  *payload = NULL;
	size_t payload_len = 0;
	const char **mkeys = NULL;
	size_t *mkeys_len = NULL;
	char *res_key = NULL;
	size_t res_key_len = 0;
	uint32_t flags;
	uint64_t cas = 0;
	zval *cas_tokens = NULL;
	uint64_t orig_cas_flag;
	zval *value;
	long get_flags = 0;
	int i = 0;
	zend_bool preserve_order;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|zl", &server_key,
								  &server_key_len, &keys, &cas_tokens, &get_flags) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|zl", &keys, &cas_tokens, &get_flags) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	preserve_order = (get_flags & MEMC_GET_PRESERVE_ORDER);
	num_keys  = zend_hash_num_elements(Z_ARRVAL_P(keys));
	mkeys     = safe_emalloc(num_keys, sizeof(char *), 0);
	mkeys_len = safe_emalloc(num_keys, sizeof(size_t), 0);
	array_init(return_value);

	/*
	 * Create the array of keys for libmemcached. If none of the keys were valid
	 * (strings), set bad key result code and return.
	 */
	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(keys));
		 zend_hash_get_current_data(Z_ARRVAL_P(keys), (void**)&entry) == SUCCESS;
		 zend_hash_move_forward(Z_ARRVAL_P(keys))) {

		if (Z_TYPE_PP(entry) == IS_STRING && Z_STRLEN_PP(entry) > 0) {
			mkeys[i]     = Z_STRVAL_PP(entry);
			mkeys_len[i] = Z_STRLEN_PP(entry);
			if (preserve_order) {
				add_assoc_null_ex(return_value, mkeys[i], mkeys_len[i]+1);
			}
			i++;
		}
	}

	if (i == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		zval_dtor(return_value);
		efree(mkeys);
		efree(mkeys_len);
		RETURN_FALSE;
	}

	/*
	 * Enable CAS support, but only if it is currently disabled.
	 */
	if (cas_tokens) {
		orig_cas_flag = memcached_behavior_get(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS);
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);
		}
	}

	status = memcached_mget_by_key(m_obj->memc, server_key, server_key_len, mkeys, mkeys_len, i);

	/*
	 * Restore the CAS support flag, but only if we had to turn it on.
	 */
	if (cas_tokens) {
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
		}
	}

	efree(mkeys);
	efree(mkeys_len);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}

	/*
	 * Iterate through the result set and create the result array. The CAS tokens are
	 * returned as doubles, because we cannot store potential 64-bit values in longs.
	 */
	if (cas_tokens) {
		zval_dtor(cas_tokens);
		array_init(cas_tokens);
	}

	status = MEMCACHED_SUCCESS;
	memcached_result_create(m_obj->memc, &result);
	while ((memcached_fetch_result(m_obj->memc, &result, &status)) != NULL) {

		payload     = memcached_result_value(&result);
		payload_len = memcached_result_length(&result);
		flags       = memcached_result_flags(&result);
		res_key     = memcached_result_key_value(&result);
		res_key_len = memcached_result_key_length(&result);

		MAKE_STD_ZVAL(value);

		if (php_memc_zval_from_payload(value, payload, payload_len, flags, m_obj->serializer TSRMLS_CC) < 0) {
			zval_ptr_dtor(&value);
			zval_dtor(return_value);
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			memcached_result_free(&result);
			RETURN_FALSE;
		}
		add_assoc_zval_ex(return_value, res_key, res_key_len+1, value);
		if (cas_tokens) {
			cas = memcached_result_cas(&result);
			add_assoc_double_ex(cas_tokens, res_key, res_key_len+1, (double)cas);
		}
	}

	memcached_result_free(&result);

	if (status != MEMCACHED_END && php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
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
	char *server_key = NULL;
	int   server_key_len = 0;
	zend_bool with_cas = 0;
	size_t num_keys = 0;
	zval **entry = NULL;
	const char **mkeys = NULL;
	size_t *mkeys_len = NULL;
	uint64_t orig_cas_flag;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	int i = 0;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|bf!", &server_key,
								  &server_key_len, &keys, &with_cas, &fci, &fcc) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|bf!", &keys, &with_cas,
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
	mkeys     = safe_emalloc(num_keys, sizeof(char *), 0);
	mkeys_len = safe_emalloc(num_keys, sizeof(size_t), 0);

	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(keys));
		 zend_hash_get_current_data(Z_ARRVAL_P(keys), (void**)&entry) == SUCCESS;
		 zend_hash_move_forward(Z_ARRVAL_P(keys))) {

		if (Z_TYPE_PP(entry) == IS_STRING && Z_STRLEN_PP(entry) > 0) {
			mkeys[i]     = Z_STRVAL_PP(entry);
			mkeys_len[i] = Z_STRLEN_PP(entry);
			i++;
		}
	}

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
	status = memcached_mget_by_key(m_obj->memc, server_key, server_key_len, mkeys, mkeys_len, i);

	/*
	 * Restore the CAS support flag, but only if we had to turn it on.
	 */
	if (with_cas) {
		if (orig_cas_flag == 0) {
			memcached_behavior_set(m_obj->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
		}
	}

	efree(mkeys);
	efree(mkeys_len);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
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
			if (php_memc_do_result_callback(getThis(), &fci, &fcc, &result TSRMLS_CC) < 0) {
				status = MEMCACHED_FAILURE;
				break;
			}
		}
		memcached_result_free(&result);

		/* we successfully retrieved all rows */
		if (status == MEMCACHED_END) {
			status = MEMCACHED_SUCCESS;
		}
		if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
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
	char *res_key = NULL;
	size_t res_key_len = 0;
	char  *payload = NULL;
	size_t payload_len = 0;
	zval *value;
	uint32_t flags = 0;
	uint64_t cas = 0;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	memcached_result_create(m_obj->memc, &result);
	if ((memcached_fetch_result(m_obj->memc, &result, &status)) == NULL) {
		php_memc_handle_error(i_obj, status TSRMLS_CC);
		memcached_result_free(&result);
		RETURN_FALSE;
	}

	payload     = memcached_result_value(&result);
	payload_len = memcached_result_length(&result);
	flags       = memcached_result_flags(&result);
	res_key     = memcached_result_key_value(&result);
	res_key_len = memcached_result_key_length(&result);
	cas 		= memcached_result_cas(&result);

	MAKE_STD_ZVAL(value);

	if (php_memc_zval_from_payload(value, payload, payload_len, flags, m_obj->serializer TSRMLS_CC) < 0) {
		memcached_result_free(&result);
		zval_ptr_dtor(&value);
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_stringl_ex(return_value, ZEND_STRS("key"), res_key, res_key_len, 1);
	add_assoc_zval_ex(return_value, ZEND_STRS("value"), value);
	add_assoc_double_ex(return_value, ZEND_STRS("cas"), (double)cas);

	memcached_result_free(&result);
}
/* }}} */

/* {{{ Memcached::fetchAll()
   Returns all the results from a previous delayed request */
PHP_METHOD(Memcached, fetchAll)
{
	char *res_key = NULL;
	size_t res_key_len = 0;
	char  *payload = NULL;
	size_t payload_len = 0;
	zval *value, *entry;
	uint32_t flags;
	uint64_t cas = 0;
	memcached_result_st result;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
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
		cas 		= memcached_result_cas(&result);

		MAKE_STD_ZVAL(value);

		if (php_memc_zval_from_payload(value, payload, payload_len, flags, m_obj->serializer TSRMLS_CC) < 0) {
			memcached_result_free(&result);
			zval_ptr_dtor(&value);
			zval_dtor(return_value);
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			RETURN_FALSE;
		}

		MAKE_STD_ZVAL(entry);
		array_init(entry);
		add_assoc_stringl_ex(entry, ZEND_STRS("key"), res_key, res_key_len, 1);
		add_assoc_zval_ex(entry, ZEND_STRS("value"), value);
		add_assoc_double_ex(entry, ZEND_STRS("cas"), (double)cas);
		add_next_index_zval(return_value, entry);
	}

	memcached_result_free(&result);

	if (status != MEMCACHED_END && php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::set(string key, mixed value [, int expiration ])
   Sets the value for the given key */
PHP_METHOD(Memcached, set)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_SET, 0);
}
/* }}} */

/* {{{ Memcached::setByKey(string server_key, string key, mixed value [, int expiration ])
   Sets the value for the given key on the server identified by the server key */
PHP_METHOD(Memcached, setByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_SET, 1);
}
/* }}} */

/* {{{ Memcached::setMulti(array items [, int expiration ])
   Sets the keys/values specified in the items array */
PHP_METHOD(Memcached, setMulti)
{
	php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::setMultiByKey(string server_key, array items [, int expiration ])
   Sets the keys/values specified in the items array on the server identified by the given server key */
PHP_METHOD(Memcached, setMultiByKey)
{
	php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_setMulti_impl */
static void php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *entries;
	char *server_key = NULL;
	int   server_key_len = 0;
	time_t expiration = 0;
	zval **entry;
	char *str_key;
	uint  str_key_len;
	ulong num_key;
	char  *payload;
	size_t payload_len;
	uint32_t flags = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|l", &server_key,
								  &server_key_len, &entries, &expiration) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &entries, &expiration) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(entries));
		 zend_hash_get_current_data(Z_ARRVAL_P(entries), (void**)&entry) == SUCCESS;
		 zend_hash_move_forward(Z_ARRVAL_P(entries))) {

		if (zend_hash_get_current_key_ex(Z_ARRVAL_P(entries), &str_key, &str_key_len, &num_key, 0, NULL) != HASH_KEY_IS_STRING) {
			continue;
		}

		flags = 0;
		if (m_obj->compression) {
			flags |= MEMC_VAL_COMPRESSED;
		}

		payload = php_memc_zval_to_payload(*entry, &payload_len, &flags, m_obj->serializer TSRMLS_CC);
		if (payload == NULL) {
			i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
			RETURN_FALSE;
		}

		if (!by_key) {
			server_key     = str_key;
			server_key_len = str_key_len-1;
		}
		status = memcached_set_by_key(m_obj->memc, server_key, server_key_len, str_key,
									  str_key_len-1, payload, payload_len, expiration, flags);
		efree(payload);

		if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::add(string key, mixed value [, int expiration ])
   Sets the value for the given key, failing if the key already exists */
PHP_METHOD(Memcached, add)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_ADD, 0);
}
/* }}} */

/* {{{ Memcached::addByKey(string server_key, string key, mixed value [, int expiration ])
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

/* {{{ Memcached::replace(string key, mixed value [, int expiration ])
   Replaces the value for the given key, failing if the key doesn't exist */
PHP_METHOD(Memcached, replace)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_REPLACE, 0);
}
/* }}} */

/* {{{ Memcached::replaceByKey(string server_key, string key, mixed value [, int expiration ])
   Replaces the value for the given key on the server identified by the server key, failing if the key doesn't exist */
PHP_METHOD(Memcached, replaceByKey)
{
	php_memc_store_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, MEMC_OP_REPLACE, 1);
}
/* }}} */

/* {{{ -- php_memc_store_impl */
static void php_memc_store_impl(INTERNAL_FUNCTION_PARAMETERS, int op, zend_bool by_key)
{
	char *key = NULL;
	int   key_len = 0;
	char *server_key = NULL;
	int   server_key_len = 0;
	char *s_value = NULL;
	int   s_value_len = 0;
	zval *value;
	time_t expiration = 0;
	char  *payload;
	size_t payload_len;
	uint32_t flags = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &server_key,
									  &server_key_len, &key, &key_len, &s_value, &s_value_len, &expiration) == FAILURE) {
				return;
			}
			MAKE_STD_ZVAL(value);
			ZVAL_STRINGL(value, s_value, s_value_len, 1);
		} else {
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssz|l", &server_key,
									  &server_key_len, &key, &key_len, &value, &expiration) == FAILURE) {
				return;
			}
		}
	} else {
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len,
									  &s_value, &s_value_len) == FAILURE) {
				return;
			}
			MAKE_STD_ZVAL(value);
			ZVAL_STRINGL(value, s_value, s_value_len, 1);
		} else {
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &key_len,
									  &value, &expiration) == FAILURE) {
				return;
			}
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key_len == 0) {
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
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot append/prepend with compression turned on");
			return;
		}
		flags |= MEMC_VAL_COMPRESSED;
	}

	payload = php_memc_zval_to_payload(value, &payload_len, &flags, m_obj->serializer TSRMLS_CC);
	if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
		zval_ptr_dtor(&value);
	}
	if (payload == NULL) {
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FALSE;
	}

	switch (op) {
		case MEMC_OP_SET:
			if (!server_key) {
				status = memcached_set(m_obj->memc, key, key_len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_set_by_key(m_obj->memc, server_key, server_key_len, key,
										  key_len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_ADD:
			if (!server_key) {
				status = memcached_add(m_obj->memc, key, key_len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_add_by_key(m_obj->memc, server_key, server_key_len, key,
										  key_len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_REPLACE:
			if (!server_key) {
				status = memcached_replace(m_obj->memc, key, key_len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_replace_by_key(m_obj->memc, server_key, server_key_len, key,
										      key_len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_APPEND:
			if (!server_key) {
				status = memcached_append(m_obj->memc, key, key_len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_append_by_key(m_obj->memc, server_key, server_key_len, key,
											 key_len, payload, payload_len, expiration, flags);
			}
			break;

		case MEMC_OP_PREPEND:
			if (!server_key) {
				status = memcached_prepend(m_obj->memc, key, key_len, payload, payload_len, expiration, flags);
			} else {
				status = memcached_prepend_by_key(m_obj->memc, server_key, server_key_len, key,
											  key_len, payload, payload_len, expiration, flags);
			}
			break;

		default:
			/* not reached */
			assert(0);
			break;
	}

	efree(payload);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::cas(double cas_token, string key, mixed value [, int expiration ])
   Sets the value for the given key, failing if the cas_token doesn't match the one in memcache */
PHP_METHOD(Memcached, cas)
{
	php_memc_cas_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::casByKey(double cas_token, string server_key, string key, mixed value [, int expiration ])
   Sets the value for the given key on the server identified by the server_key, failing if the cas_token doesn't match the one in memcache */
PHP_METHOD(Memcached, casByKey)
{
	php_memc_cas_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ -- php_memc_cas_impl */
static void php_memc_cas_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	double cas_d;
	uint64_t cas;
	char *key = NULL;
	int   key_len = 0;
	char *server_key = NULL;
	int   server_key_len = 0;
	zval *value;
	time_t expiration = 0;
	char  *payload;
	size_t payload_len;
	uint32_t flags = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dssz|l", &cas_d, &server_key,
								  &server_key_len, &key, &key_len, &value, &expiration) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dsz|l", &cas_d, &key, &key_len,
								  &value, &expiration) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key_len == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	DVAL_TO_LVAL(cas_d, cas);

	if (m_obj->compression) {
		flags |= MEMC_VAL_COMPRESSED;
	}

	payload = php_memc_zval_to_payload(value, &payload_len, &flags, m_obj->serializer TSRMLS_CC);
	if (payload == NULL) {
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FALSE;
	}
	status = memcached_cas_by_key(m_obj->memc, server_key, server_key_len, key, key_len,
								  payload, payload_len, expiration, flags, cas);
	efree(payload);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
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
	char *key = NULL;
	int   key_len = 0;
	char *server_key = NULL;
	int   server_key_len = 0;
	time_t expiration = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|l", &server_key,
								  &server_key_len, &key, &key_len, &expiration) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len,
								  &expiration) == FAILURE) {
			return;
		}
		server_key     = key;
		server_key_len = key_len;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key_len == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	status = memcached_delete_by_key(m_obj->memc, server_key, server_key_len, key,
									 key_len, expiration);

	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ -- php_memc_deleteMulti_impl */
static void php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *entries;
	char *server_key = NULL;
	int   server_key_len = 0;
	time_t expiration = 0;
	zval **entry;

	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|l", &server_key,
								  &server_key_len, &entries, &expiration) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &entries, &expiration) == FAILURE) {
			return;
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	array_init(return_value);
	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(entries));
		 zend_hash_get_current_data(Z_ARRVAL_P(entries), (void**)&entry) == SUCCESS;
		 zend_hash_move_forward(Z_ARRVAL_P(entries))) {

		if (Z_TYPE_PP(entry) != IS_STRING || Z_STRLEN_PP(entry) <= 0) {
			continue;
		}

		if (!by_key) {
			server_key     = Z_STRVAL_PP(entry);
			server_key_len = Z_STRLEN_PP(entry);
		}

		status = memcached_delete_by_key(m_obj->memc, server_key, server_key_len, Z_STRVAL_PP(entry), Z_STRLEN_PP(entry), expiration);

		if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
			add_assoc_long(return_value, Z_STRVAL_PP(entry), status);
		} else {
			add_assoc_bool(return_value, Z_STRVAL_PP(entry), 1);
		}
	}

	return;
}
/* }}} */

/* {{{ Memcached::increment(string key [, int delta ])
   Increments the value for the given key by delta, defaulting to 1 */
PHP_METHOD(Memcached, increment)
{
	php_memc_incdec_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ Memcached::decrement(string key [, int delta ])
   Decrements the value for the given key by delta, defaulting to 1 */
PHP_METHOD(Memcached, decrement)
{
	php_memc_incdec_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ -- php_memc_incdec_impl */
static void php_memc_incdec_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool incr)
{
	char *key = NULL;
	int   key_len = 0;
	long  offset = 1;
	uint64_t value;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len, &offset) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (key_len == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	if (offset < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "offset has to be > 0");
		RETURN_FALSE;
	}

	if (incr) {
		status = memcached_increment(m_obj->memc, key, key_len, (unsigned int)offset, &value);
	} else {
		status = memcached_decrement(m_obj->memc, key, key_len, (unsigned int)offset, &value);
	}

	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		RETURN_FALSE;
	}

	RETURN_LONG((long)value);
}
/* }}} */

/* {{{ Memcached::addServer(string hostname, int port [, int weight ])
   Adds the given memcache server to the list */
PHP_METHOD(Memcached, addServer)
{
	char *host;
	int   host_len;
	long  port, weight = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|l", &host, &host_len,
							  &port, &weight) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (host[0] == '/') { /* unix domain socket */
		status = memcached_server_add_unix_socket_with_weight(m_obj->memc, host, weight);
	} else {
		status = memcached_server_add_with_weight(m_obj->memc, host, port, weight);
	}

	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
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
	zval **entry;
	zval **z_host, **z_port, **z_weight = NULL;
	uint32_t weight = 0;
	int   entry_size, i = 0;
	memcached_server_st *list = NULL;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &servers) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(servers)), i = 0;
		 zend_hash_get_current_data(Z_ARRVAL_P(servers), (void **)&entry) == SUCCESS;
		 zend_hash_move_forward(Z_ARRVAL_P(servers)), i++) {

		if (Z_TYPE_PP(entry) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "server list entry #%d is not an array", i+1);
			continue;
		}

		entry_size = zend_hash_num_elements(Z_ARRVAL_PP(entry));

		if (entry_size > 1) {
			zend_hash_internal_pointer_reset(Z_ARRVAL_PP(entry));

			/* Check that we have a host */
			if (zend_hash_get_current_data(Z_ARRVAL_PP(entry), (void **)&z_host) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not get server host for entry #%d", i+1);
				continue;
			}

			/* Check that we have a port */
			if (zend_hash_move_forward(Z_ARRVAL_PP(entry)) == FAILURE ||
				zend_hash_get_current_data(Z_ARRVAL_PP(entry), (void **)&z_port) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not get server port for entry #%d", i+1);
				continue;
			}

			convert_to_string_ex(z_host);
			convert_to_long_ex(z_port);

			weight = 0;
			if (entry_size > 2) {
				/* Try to get weight */
				if (zend_hash_move_forward(Z_ARRVAL_PP(entry)) == FAILURE ||
					zend_hash_get_current_data(Z_ARRVAL_PP(entry), (void **)&z_weight) == FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not get server weight for entry #%d", i+1);
				}

				convert_to_long_ex(z_weight);
				weight = Z_LVAL_PP(z_weight);
			}

			list = memcached_server_list_append_with_weight(list, Z_STRVAL_PP(z_host),
															Z_LVAL_PP(z_port), weight, &status);

			if (php_memc_handle_error(i_obj, status TSRMLS_CC) == 0) {
				continue;
			}
		}

		/* catch-all for all errors */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not add entry #%d to the server list", i+1);
	}

	status = memcached_server_push(m_obj->memc, list);
	memcached_server_list_free(list);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::getServerList()
   Returns the list of the memcache servers in use */
PHP_METHOD(Memcached, getServerList)
{
	memcached_server_st *servers;
	unsigned int i, servers_count;
	zval *array;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);
	servers = memcached_server_list(m_obj->memc);
	if (servers == NULL) {
		return;
	}
	servers_count = memcached_server_count(m_obj->memc);

	for (i = 0; i < servers_count; i++) {
		MAKE_STD_ZVAL(array);
		array_init(array);
		add_assoc_string(array, "host", servers[i].hostname, 1);
		add_assoc_long(array, "port", servers[i].port);
		add_assoc_long(array, "weight", servers[i].weight);
		add_next_index_zval(return_value, array);
	}
}
/* }}} */

/* {{{ Memcached::getServerByKey(string server_key)
   Returns the server identified by the given server key */
PHP_METHOD(Memcached, getServerByKey)
{
	char *server_key;
	int   server_key_len;
	memcached_server_st *server;
	memcached_return error;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &server_key, &server_key_len) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	if (server_key_len == 0) {
		i_obj->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		RETURN_FALSE;
	}

	server = memcached_server_by_key(m_obj->memc, server_key, server_key_len, &error);
	if (server == NULL) {
		php_memc_handle_error(i_obj, error TSRMLS_CC);
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string(return_value, "host", server->hostname, 1);
	add_assoc_long(return_value, "port", server->port);
	add_assoc_long(return_value, "weight", server->weight);
	memcached_server_free(server);
}
/* }}} */

/* {{{ Memcached::getStats()
   Returns statistics for the memcache servers */
PHP_METHOD(Memcached, getStats)
{
    memcached_stat_st *stats;
	memcached_server_st *servers;
	unsigned int i, servers_count;
	memcached_return status;
	char *hostport = NULL;
	int hostport_len;
	zval *entry;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	stats = memcached_stat(m_obj->memc, NULL, &status);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		RETURN_FALSE;
	}

	array_init(return_value);
	servers = memcached_server_list(m_obj->memc);
	servers_count = memcached_server_count(m_obj->memc);
	if (servers == NULL) {
		return;
	}

	for (i = 0; i < servers_count; i++) {
		hostport_len = spprintf(&hostport, 0, "%s:%u", servers[i].hostname, servers[i].port);

		MAKE_STD_ZVAL(entry);
		array_init(entry);

		add_assoc_long(entry, "pid", stats[i].pid);
		add_assoc_long(entry, "uptime", stats[i].uptime);
		add_assoc_long(entry, "threads", stats[i].threads);
		add_assoc_long(entry, "time", stats[i].time);
		add_assoc_long(entry, "pointer_size", stats[i].pointer_size);
		add_assoc_long(entry, "rusage_user_seconds", stats[i].rusage_user_seconds);
		add_assoc_long(entry, "rusage_user_microseconds", stats[i].rusage_user_microseconds);
		add_assoc_long(entry, "rusage_system_seconds", stats[i].rusage_system_seconds);
		add_assoc_long(entry, "rusage_system_microseconds", stats[i].rusage_system_microseconds);
		add_assoc_long(entry, "curr_items", stats[i].curr_items);
		add_assoc_long(entry, "total_items", stats[i].total_items);
		add_assoc_long(entry, "limit_maxbytes", stats[i].limit_maxbytes);
		add_assoc_long(entry, "curr_connections", stats[i].curr_connections);
		add_assoc_long(entry, "total_connections", stats[i].total_connections);
		add_assoc_long(entry, "connection_structures", stats[i].connection_structures);
		add_assoc_long(entry, "bytes", stats[i].bytes);
		add_assoc_long(entry, "cmd_get", stats[i].cmd_get);
		add_assoc_long(entry, "cmd_set", stats[i].cmd_set);
		add_assoc_long(entry, "get_hits", stats[i].get_hits);
		add_assoc_long(entry, "get_misses", stats[i].get_misses);
		add_assoc_long(entry, "evictions", stats[i].evictions);
		add_assoc_long(entry, "bytes_read", stats[i].bytes_read);
		add_assoc_long(entry, "bytes_written", stats[i].bytes_written);
		add_assoc_stringl(entry, "version", stats[i].version, strlen(stats[i].version), 1);

		add_assoc_zval_ex(return_value, hostport, hostport_len+1, entry);
		efree(hostport);
	}

	memcached_stat_free(m_obj->memc, stats);
}
/* }}} */

/* {{{ Memcached::getVersion()
   Returns the version of each memcached server in the pool */
PHP_METHOD(Memcached, getVersion)
{
	memcached_server_st *servers;
	unsigned int i, servers_count;
	char *hostport = NULL;
	char version[16];
	int hostport_len, version_len;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);
	servers = memcached_server_list(m_obj->memc);
	servers_count = memcached_server_count(m_obj->memc);
	if (servers == NULL) {
		return;
	}

	status = memcached_version(m_obj->memc);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}

	for (i = 0; i < servers_count; i++) {
		hostport_len = spprintf(&hostport, 0, "%s:%u", servers[i].hostname, servers[i].port);
		version_len = snprintf(version, sizeof(version), "%d.%d.%d",
							   servers[i].major_version, servers[i].minor_version,
							   servers[i].micro_version);

		add_assoc_stringl_ex(return_value, hostport, hostport_len+1, version, version_len, 1);
		efree(hostport);
	}
}
/* }}} */

/* {{{ Memcached::getAllKeys()
	Returns the keys stored on all the servers */
static memcached_return php_memc_dump_func_callback(memcached_st *ptr __attribute__((unused)), \
	const char *key, size_t key_length, void *context)
{
	zval *ctx = (zval*) context;
	add_next_index_string(ctx, (char*) key, 1);

	return MEMCACHED_SUCCESS;
}

PHP_METHOD(Memcached, getAllKeys)
{
	memcached_return rc;
	memcached_dump_func callback[1];
	MEMC_METHOD_INIT_VARS;

	callback[0] = &php_memc_dump_func_callback;
	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);
	rc = memcached_dump(m_obj->memc, callback, return_value, 1);
	if (php_memc_handle_error(i_obj, rc TSRMLS_CC) < 0) {
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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &delay) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	i_obj->rescode = MEMCACHED_SUCCESS;

	status = memcached_flush(m_obj->memc, delay);
	if (php_memc_handle_error(i_obj, status TSRMLS_CC) < 0) {
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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &option) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	switch (option) {
		case MEMC_OPT_COMPRESSION:
			RETURN_BOOL(m_obj->compression);

		case MEMC_OPT_PREFIX_KEY:
		{
			memcached_return retval;
			char *result;

			result = memcached_callback_get(m_obj->memc, MEMCACHED_CALLBACK_PREFIX_KEY, &retval);
			if (retval == MEMCACHED_SUCCESS) {
				RETURN_STRING(result, 1);
			} else {
				RETURN_EMPTY_STRING();
			}
		}

		case MEMC_OPT_SERIALIZER:
			RETURN_LONG((long)m_obj->serializer);
			break;

		case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
		case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
			if (memcached_server_count(m_obj->memc) == 0) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "no servers defined");
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

static int php_memc_set_option(php_memc_t *i_obj, long option, zval *value TSRMLS_DC)
{
	memcached_behavior flag;
	struct memc_obj *m_obj = i_obj->obj;

    switch (option) {
        case MEMC_OPT_COMPRESSION:
            convert_to_long(value);
            m_obj->compression = Z_LVAL_P(value) ? 1 : 0;
            break;

        case MEMC_OPT_PREFIX_KEY:
        {
            char *key;
            convert_to_string(value);
            if (Z_STRLEN_P(value) == 0) {
                key = NULL;
            } else {
                key = Z_STRVAL_P(value);
            }
            if (memcached_callback_set(m_obj->memc, MEMCACHED_CALLBACK_PREFIX_KEY, key) == MEMCACHED_BAD_KEY_PROVIDED) {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "bad key provided");
                return 0;
            }
            break;
        }

        case MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED:
        	flag = (memcached_behavior) option;

            convert_to_long(value);
            if (memcached_behavior_set(m_obj->memc, flag, (uint64_t)Z_LVAL_P(value)) == MEMCACHED_FAILURE) {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "error setting memcached option");
                return 0;
            }

            /*
             * This is necessary because libmemcached does not reset hash/distribution
             * options on false case, like it does for MEMCACHED_BEHAVIOR_KETAMA
             * (non-weighted) case. We have to clean up ourselves.
             */
             if (!Z_LVAL_P(value)) {
                 m_obj->memc->hash = 0;
                 m_obj->memc->distribution = 0;
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
                    /* php serializer */
                    if (Z_LVAL_P(value) == SERIALIZER_PHP) {
                        m_obj->serializer = SERIALIZER_PHP;
                    } else {
                        m_obj->serializer = SERIALIZER_PHP;
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid serializer provided");
						return 0;
                    }
                    break;
            }

        default:
                /*
                 * Assume that it's a libmemcached behavior option.
                 */
                 flag = (memcached_behavior) option;
                 convert_to_long(value);
                 if (memcached_behavior_set(m_obj->memc, flag, (uint64_t)Z_LVAL_P(value)) != MEMCACHED_SUCCESS) {
                     php_error_docref(NULL TSRMLS_CC, E_WARNING, "error setting memcached option");
                     return 0;
                 }
                 break;
	}

	return 1;
}

/* {{{ Memcached::setOptions(array options)
   Sets the value for the given option constant */
static PHP_METHOD(Memcached, setOptions)
{
	zval *options;
	zend_bool ok = 1;
	uint key_len;
	char *key;
	ulong key_index;
	zval **value;

	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &options) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(options));
		 zend_hash_get_current_data(Z_ARRVAL_P(options), (void *) &value) == SUCCESS;
		 zend_hash_move_forward(Z_ARRVAL_P(options))) {

		if (zend_hash_get_current_key_ex(Z_ARRVAL_P(options), &key, &key_len, &key_index, 0, NULL) == HASH_KEY_IS_LONG) {
			if (!php_memc_set_option(i_obj, (long) key_index, *value TSRMLS_CC)) {
				ok = 0;
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid configuration option");
		}
	}

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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz/", &option, &value) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_BOOL(php_memc_set_option(i_obj, option, value TSRMLS_CC));
}
/* }}} */

/* {{{ Memcached::getResultCode()
   Returns the result code from the last operation */
static PHP_METHOD(Memcached, getResultCode)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	switch (i_obj->rescode) {
		case MEMC_RES_PAYLOAD_FAILURE:
			RETURN_STRING("PAYLOAD FAILURE", 1);
			break;

		case MEMCACHED_ERRNO:
		case MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE:
		case MEMCACHED_UNKNOWN_READ_FAILURE:
			if (i_obj->memc_errno) {
				char *str;
				int str_len;
				str_len = spprintf(&str, 0, "%s: %s", memcached_strerror(m_obj->memc, (memcached_return)i_obj->rescode),
					strerror(i_obj->memc_errno));
				RETURN_STRINGL(str, str_len, 0);
			}
			/* Fall through */
		default:
			RETURN_STRING(memcached_strerror(m_obj->memc, (memcached_return)i_obj->rescode), 1);
			break;
	}

}
/* }}} */

/* {{{ Memcached::isPersistent()
   Returns the true if instance uses a persistent connection */
static PHP_METHOD(Memcached, isPersistent)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
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
static void php_memc_destroy(struct memc_obj *m_obj, bool persistent TSRMLS_DC)
{
	if (m_obj->memc) {
		memcached_free(m_obj->memc);
	}

	pefree(m_obj, persistent);
}

static void php_memc_free_storage(php_memc_t *i_obj TSRMLS_DC)
{
	zend_object_std_dtor(&i_obj->zo TSRMLS_CC);

	if (i_obj->obj && !i_obj->is_persistent) {
		php_memc_destroy(i_obj->obj, false TSRMLS_CC);
	}

	i_obj->obj = NULL;
	efree(i_obj);
}

zend_object_value php_memc_new(zend_class_entry *ce TSRMLS_DC)
{
    zend_object_value retval;
    php_memc_t *i_obj;
    zval *tmp;

    i_obj = ecalloc(1, sizeof(*i_obj));
	zend_object_std_init( &i_obj->zo, ce TSRMLS_CC );
    zend_hash_copy(i_obj->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(i_obj, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t)php_memc_free_storage, NULL TSRMLS_CC);
    retval.handlers = zend_get_std_object_handlers();

    return retval;
}

ZEND_RSRC_DTOR_FUNC(php_memc_dtor)
{
    if (rsrc->ptr) {
		struct memc_obj *m_obj = (struct memc_obj *)rsrc->ptr;
		php_memc_destroy(m_obj, true TSRMLS_CC);
        rsrc->ptr = NULL;
    }
}
/* }}} */

/* {{{ internal API functions */
static int php_memc_handle_error(php_memc_t *i_obj, memcached_return status TSRMLS_DC)
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

		default:
			i_obj->rescode = status;
			/* Hnngghgh! */
			i_obj->memc_errno = i_obj->obj->memc->cached_errno;
			result = -1;
			break;
	}

	return result;
}

static char *php_memc_zval_to_payload(zval *value, size_t *payload_len, uint32_t *flags, enum memcached_serializer serializer TSRMLS_DC)
{
	char *payload;
	smart_str buf = {0};

	switch (Z_TYPE_P(value)) {

		case IS_STRING:
			smart_str_appendl(&buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_STRING);
			break;

		case IS_LONG:
		case IS_DOUBLE:
		case IS_BOOL:
		{
			zval value_copy;

			value_copy = *value;
			zval_copy_ctor(&value_copy);
			convert_to_string(&value_copy);
			smart_str_appendl(&buf, Z_STRVAL(value_copy), Z_STRLEN(value_copy));
			zval_dtor(&value_copy);

			*flags &= ~MEMC_VAL_COMPRESSED;
			if (Z_TYPE_P(value) == IS_LONG) {
				MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_LONG);
			} else if (Z_TYPE_P(value) == IS_DOUBLE) {
				MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_DOUBLE);
			} else if (Z_TYPE_P(value) == IS_BOOL) {
				MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_BOOL);
			}
			break;
		}

		default:
			switch (serializer) {
#ifdef HAVE_MEMCACHED_IGBINARY
				case SERIALIZER_IGBINARY:
					igbinary_serialize((uint8_t **) &buf.c, &buf.len, value);
					MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_IGBINARY);
					break;
#endif

#ifdef HAVE_JSON_API
				case SERIALIZER_JSON:
				case SERIALIZER_JSON_ARRAY:
				{

#if HAVE_JSON_API_5_2
					php_json_encode(&buf, value TSRMLS_CC);
#elif HAVE_JSON_API_5_3
					php_json_encode(&buf, value, 0 TSRMLS_CC); //options
#endif
					buf.c[buf.len] = 0;
					MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_JSON);
					break;
				}
#endif
				default:
				{
					php_serialize_data_t var_hash;
					PHP_VAR_SERIALIZE_INIT(var_hash);
					php_var_serialize(&buf, &value, &var_hash TSRMLS_CC);
					PHP_VAR_SERIALIZE_DESTROY(var_hash);

					if (!buf.c) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not serialize value");
						smart_str_free(&buf);
						return NULL;
					}

					MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_SERIALIZED);
					break;
				}
			}

			break;
	}

	/* turn off compression for values below the threshold */
	if ((*flags & MEMC_VAL_COMPRESSED) && buf.len < MEMC_G(compression_threshold)) {
		*flags &= ~MEMC_VAL_COMPRESSED;
	}

	if (*flags & MEMC_VAL_COMPRESSED) {
		/* status */
		zend_bool compress_status = 0;

		/* Additional 5% for the data */
		unsigned long payload_comp_len = (unsigned long)((buf.len + 1.05) + 1);
		char *payload_comp = emalloc(payload_comp_len + sizeof(uint32_t));
		payload = payload_comp;
		memcpy(payload_comp, &buf.len, sizeof(uint32_t));
		payload_comp += sizeof(uint32_t);

		if (MEMC_G(compression_type_real) == COMPRESSION_TYPE_FASTLZ) {
			compress_status = ((payload_comp_len = fastlz_compress(buf.c, buf.len, payload_comp)) > 0);
			*flags |= MEMC_VAL_COMPRESSION_FASTLZ;
		} else if (MEMC_G(compression_type_real) == COMPRESSION_TYPE_ZLIB) {
			compress_status = (compress(payload_comp, &payload_comp_len, buf.c, buf.len) == Z_OK);
			*flags |= MEMC_VAL_COMPRESSION_ZLIB;
		}

		if (!compress_status) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not compress value");
			efree(payload);
			smart_str_free(&buf);
			return NULL;
		}

		/* Check that we are above ratio */
		if (buf.len > (payload_comp_len * MEMC_G(compression_factor))) {
			*payload_len = payload_comp_len + sizeof(uint32_t);
			payload[*payload_len] = 0;
		} else {
			/* Store plain value */
			*flags &= ~MEMC_VAL_COMPRESSED;
			*payload_len = buf.len;
			memcpy(payload, buf.c, buf.len);
			payload[buf.len] = 0;
		}
	} else {
		*payload_len = buf.len;
		payload = estrndup(buf.c, buf.len);
	}

	smart_str_free(&buf);
	return payload;
}

/* The caller MUST free the payload */
static int php_memc_zval_from_payload(zval *value, char *payload, size_t payload_len, uint32_t flags, enum memcached_serializer serializer TSRMLS_DC)
{
	/*
	   A NULL payload is completely valid if length is 0, it is simply empty.
	 */
	zend_bool payload_emalloc = 0;
	char *buffer = NULL;

	if (payload == NULL && payload_len > 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Could not handle non-existing value of length %zu", payload_len);
		return -1;
	} else if (payload == NULL) {
		if (MEMC_VAL_GET_TYPE(flags) == MEMC_VAL_IS_BOOL) {
			ZVAL_FALSE(value);
		} else {
			ZVAL_EMPTY_STRING(value);
		}
		return 0;
	}

	if (flags & MEMC_VAL_COMPRESSED) {
		uint32_t len;
		unsigned long length;
		zend_bool decompress_status = 0;

		/* Stored with newer memcached extension? */
		if (flags & MEMC_VAL_COMPRESSION_FASTLZ || flags & MEMC_VAL_COMPRESSION_ZLIB) {
			/* This is copied from Ilia's patch */
			memcpy(&len, payload, sizeof(uint32_t));
			buffer = emalloc(len + 1);
			payload_len -= sizeof(uint32_t);
			payload += sizeof(uint32_t);
			length = len;

			if (flags & MEMC_VAL_COMPRESSION_FASTLZ) {
				decompress_status = ((length = fastlz_decompress(payload, payload_len, buffer, len)) > 0);
			} else if (flags & MEMC_VAL_COMPRESSION_ZLIB) {
				decompress_status = (uncompress(buffer, &length, payload, payload_len) == Z_OK);
			}
		}

		/* Fall back to 'old style decompression' */
		if (!decompress_status) {
			unsigned int factor = 1, maxfactor = 16;
			int status;

			do {
				length = (unsigned long)payload_len * (1 << factor++);
				buffer = erealloc(buffer, length + 1);
				memset(buffer, 0, length + 1);
				status = uncompress((Bytef *)buffer, (uLongf *)&length, (const Bytef *)payload, payload_len);
			} while ((status==Z_BUF_ERROR) && (factor < maxfactor));

			if (status == Z_OK) {
				decompress_status = 1;
			}
		}

		if (!decompress_status) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not decompress value");
			efree(buffer);
			return -1;
		}
		payload = buffer;
		payload_len = length;
		payload_emalloc = 1;
	}

	payload[payload_len] = 0;

	switch (MEMC_VAL_GET_TYPE(flags)) {

		case MEMC_VAL_IS_STRING:
			if ((flags & MEMC_VAL_COMPRESSED) && payload_emalloc) {
				ZVAL_STRINGL(value, payload, payload_len, 0);
			} else {
				ZVAL_STRINGL(value, payload, payload_len, 1);
			}
			break;
		case MEMC_VAL_IS_LONG:
		{
			long lval = strtol(payload, NULL, 10);
			ZVAL_LONG(value, lval);
			break;
		}
		case MEMC_VAL_IS_DOUBLE:
		{
			double dval = zend_strtod(payload, NULL);
			ZVAL_DOUBLE(value, dval);
			break;
		}
		case MEMC_VAL_IS_BOOL:
			ZVAL_BOOL(value, payload_len > 0 && payload[0] == '1');
			break;

		case MEMC_VAL_IS_SERIALIZED:
		{
			const char *payload_tmp = payload;
			php_unserialize_data_t var_hash;

			PHP_VAR_UNSERIALIZE_INIT(var_hash);
			if (!php_var_unserialize(&value, (const unsigned char **)&payload_tmp, (const unsigned char *)payload_tmp + payload_len, &var_hash TSRMLS_CC)) {
				ZVAL_FALSE(value);
				PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not unserialize value");
				goto my_error;
			}
			PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
			break;
		}

		case MEMC_VAL_IS_IGBINARY:
#ifdef HAVE_MEMCACHED_IGBINARY
			if (igbinary_unserialize((uint8_t *)payload, payload_len, &value)) {
				ZVAL_FALSE(value);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not unserialize value with igbinary");
				goto my_error;
			}
#else
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not unserialize value, no igbinary support");
			goto my_error;
#endif
			break;

		case MEMC_VAL_IS_JSON:
#ifdef HAVE_JSON_API
# if HAVE_JSON_API_5_2
			php_json_decode(value, payload, payload_len, (serializer == SERIALIZER_JSON_ARRAY) TSRMLS_CC);
# elif HAVE_JSON_API_5_3
			php_json_decode(value, payload, payload_len, (serializer == SERIALIZER_JSON_ARRAY), JSON_PARSER_DEFAULT_DEPTH TSRMLS_CC);
# endif
#else
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not unserialize value, no json support");
			goto my_error;
#endif
			break;

		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unknown payload type");
			goto my_error;
	}
	return 0;

my_error:
	if ((flags & MEMC_VAL_COMPRESSED) && payload_emalloc) {
		efree(payload);
	}
	return -1;
}

static int php_memc_list_entry(void)
{
	return le_memc;
}

static void php_memc_init_globals(zend_php_memcached_globals *php_memcached_globals_p TSRMLS_DC)
{
#if HAVE_MEMCACHED_SESSION
	MEMC_G(sess_locking_enabled) = 1;
	MEMC_G(sess_prefix) = NULL;
	MEMC_G(sess_lock_wait) = 0;
	MEMC_G(sess_locked) = 0;
	MEMC_G(sess_lock_key) = NULL;
	MEMC_G(sess_lock_key_len) = 0;
#endif
#ifdef HAVE_MEMCACHE_IGBINARY
	MEMC_G(serializer) = SERIALIZER_IGBINARY;
#else
	MEMC_G(serializer) = SERIALIZER_PHP;
#endif
	MEMC_G(compression_type) = NULL;
	MEMC_G(compression_type_real) = COMPRESSION_TYPE_ZLIB;
	MEMC_G(compression_factor) = 1.30;
}

static void php_memc_destroy_globals(zend_php_memcached_globals *php_memcached_globals_p TSRMLS_DC)
{
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
zend_class_entry *php_memc_get_exception_base(int root TSRMLS_DC)
{
#if HAVE_SPL
	if (!root) {
		if (!spl_ce_RuntimeException) {
			zend_class_entry **pce;

			if (zend_hash_find(CG(class_table), "runtimeexception",
							   sizeof("RuntimeException"), (void **) &pce) == SUCCESS) {
				spl_ce_RuntimeException = *pce;
				return *pce;
			}
		} else {
			return spl_ce_RuntimeException;
		}
	}
#endif
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}

static memcached_return php_memc_do_cache_callback(zval *zmemc_obj, zend_fcall_info *fci,
												   zend_fcall_info_cache *fcc, char *key,
												   size_t key_len, zval *value TSRMLS_DC)
{
	char *payload = NULL;
	size_t payload_len = 0;
	zval **params[3];
	zval *retval;
	zval *z_key;
	uint32_t flags = 0;
	memcached_return rc;
    php_memc_t* i_obj;
	memcached_return status = MEMCACHED_SUCCESS;

	MAKE_STD_ZVAL(z_key);
	ZVAL_STRINGL(z_key, key, key_len, 1);
	ZVAL_NULL(value);

	params[0] = &zmemc_obj;
	params[1] = &z_key;
	params[2] = &value;

	fci->retval_ptr_ptr = &retval;
	fci->params = params;
	fci->param_count = 3;

	if (zend_call_function(fci, fcc TSRMLS_CC) == SUCCESS && retval) {
		i_obj = (php_memc_t *) zend_object_store_get_object(zmemc_obj TSRMLS_CC);
		struct memc_obj *m_obj = i_obj->obj;

		convert_to_boolean(retval);
		if (Z_BVAL_P(retval) == 1) {
			payload = php_memc_zval_to_payload(value, &payload_len, &flags, m_obj->serializer TSRMLS_CC);
			if (payload == NULL) {
				status = (memcached_return)MEMC_RES_PAYLOAD_FAILURE;
			} else {
				rc = memcached_set(m_obj->memc, key, key_len, payload, payload_len, 0, flags);
				if (rc == MEMCACHED_SUCCESS || rc == MEMCACHED_BUFFERED) {
					status = rc;
				}
				efree(payload);
			}
		} else {
			status = MEMCACHED_NOTFOUND;
		}

		zval_ptr_dtor(&retval);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not invoke cache callback");
		status = MEMCACHED_FAILURE;
	}

	zval_ptr_dtor(&z_key);

	return status;
}

static int php_memc_do_result_callback(zval *zmemc_obj, zend_fcall_info *fci,
									   zend_fcall_info_cache *fcc,
									   memcached_result_st *result TSRMLS_DC)
{
	char *res_key = NULL;
	size_t res_key_len = 0;
	char  *payload = NULL;
	size_t payload_len = 0;
	zval *value, *retval = NULL;
	uint64_t cas = 0;
	zval **params[2];
	zval *z_result;
	uint32_t flags = 0;
	int rc = 0;
    php_memc_t *i_obj = NULL;

	params[0] = &zmemc_obj;
	params[1] = &z_result;

	fci->retval_ptr_ptr = &retval;
	fci->params = params;
	fci->param_count = 2;

	payload     = memcached_result_value(result);
	payload_len = memcached_result_length(result);
	flags       = memcached_result_flags(result);
	res_key     = memcached_result_key_value(result);
	res_key_len = memcached_result_key_length(result);
	cas 		= memcached_result_cas(result);

	MAKE_STD_ZVAL(value);

	i_obj = (php_memc_t *) zend_object_store_get_object(zmemc_obj TSRMLS_CC);

	if (php_memc_zval_from_payload(value, payload, payload_len, flags, i_obj->obj->serializer TSRMLS_CC) < 0) {
		zval_ptr_dtor(&value);
		i_obj->rescode = MEMC_RES_PAYLOAD_FAILURE;
		return -1;
	}

	MAKE_STD_ZVAL(z_result);
	array_init(z_result);
	add_assoc_stringl_ex(z_result, ZEND_STRS("key"), res_key, res_key_len, 1);
	add_assoc_zval_ex(z_result, ZEND_STRS("value"), value);
	add_assoc_double_ex(z_result, ZEND_STRS("cas"), (double)cas);

	if (zend_call_function(fci, fcc TSRMLS_CC) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "could not invoke result callback");
		rc = -1;
	}

	if (retval) {
		zval_ptr_dtor(&retval);
	}
	zval_ptr_dtor(&z_result);

	return rc;
}

/* }}} */

/* {{{ session support */
#if HAVE_MEMCACHED_SESSION

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

	lock_maxwait = zend_ini_long(ZEND_STRL("max_execution_time"), 0);
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

			if (memcached_callback_set(memc_sess, MEMCACHED_CALLBACK_PREFIX_KEY, MEMC_G(sess_prefix)) !=
				MEMCACHED_SUCCESS) {
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
	char *sess_key = NULL;
	int sess_key_len = 0;
	uint32_t flags = 0;
	memcached_return status;
	memcached_st *memc_sess = PS_GET_MOD_DATA();

	if (MEMC_G(sess_locking_enabled)) {
		if (php_memc_sess_lock(memc_sess, key TSRMLS_CC) < 0) {
			return FAILURE;
		}
	}

	sess_key_len = spprintf(&sess_key, 0, "%s", key);
	payload = memcached_get(memc_sess, sess_key, sess_key_len, &payload_len, &flags, &status);
	efree(sess_key);

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
	char *sess_key = NULL;
	int sess_key_len = 0;
	time_t expiration;
	int sess_lifetime;
	memcached_return status;
	memcached_st *memc_sess = PS_GET_MOD_DATA();

	sess_key_len = spprintf(&sess_key, 0, "%s", key);
	sess_lifetime = zend_ini_long(ZEND_STRL("session.gc_maxlifetime"), 0);
	if (sess_lifetime > 0) {
		expiration = time(NULL) + sess_lifetime;
	} else {
		expiration = 0;
	}
	status = memcached_set(memc_sess, sess_key, sess_key_len, val, vallen, expiration, 0);
	efree(sess_key);

	if (status == MEMCACHED_SUCCESS) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

PS_DESTROY_FUNC(memcached)
{
	char *sess_key = NULL;
	int sess_key_len = 0;
	memcached_st *memc_sess = PS_GET_MOD_DATA();

	sess_key_len = spprintf(&sess_key, 0, "%s", key);
	memcached_delete(memc_sess, sess_key, sess_key_len, 0);
	efree(sess_key);
	if (MEMC_G(sess_locking_enabled)) {
		php_memc_sess_unlock(memc_sess TSRMLS_CC);
	}

	return SUCCESS;
}

PS_GC_FUNC(memcached)
{
	return SUCCESS;
}

#endif
/* }}} */

/* {{{ methods arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, persistent_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getResultCode, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getResultMessage, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, cache_cb)
	ZEND_ARG_INFO(1, cas_token)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, cache_cb)
	ZEND_ARG_INFO(1, cas_token)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getMulti, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(1, cas_tokens)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(1, cas_tokens)
	ZEND_ARG_INFO(0, flags)
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
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setMulti, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, items, 0)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_ARRAY_INFO(0, items, 0)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_add, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_addByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_replace, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_replaceByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
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
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_casByKey, 0, 0, 4)
	ZEND_ARG_INFO(0, cas_token)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
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
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_decrement, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
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

ZEND_BEGIN_ARG_INFO(arginfo_getServerByKey, 0)
	ZEND_ARG_INFO(0, server_key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_getOption, 0)
	ZEND_ARG_INFO(0, option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_setOption, 0)
	ZEND_ARG_INFO(0, option)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_setOptions, 0)
	ZEND_ARG_INFO(0, options)
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

    MEMC_ME(addServer,          arginfo_addServer)
    MEMC_ME(addServers,         arginfo_addServers)
    MEMC_ME(getServerList,      arginfo_getServerList)
    MEMC_ME(getServerByKey,     arginfo_getServerByKey)

	MEMC_ME(getStats,           arginfo_getStats)
	MEMC_ME(getVersion,         arginfo_getVersion)
	MEMC_ME(getAllKeys,         arginfo_getAllKeys)

    MEMC_ME(flush,              arginfo_flush)

    MEMC_ME(getOption,          arginfo_getOption)
    MEMC_ME(setOption,          arginfo_setOption)
    MEMC_ME(setOptions,         arginfo_setOptions)

    MEMC_ME(isPersistent,       arginfo_isPersistent)
    MEMC_ME(isPristine,         arginfo_isPristine)
    { NULL, NULL, NULL }
};
#undef MEMC_ME
/* }}} */

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
#ifdef HAVE_SPL
    ZEND_MOD_REQUIRED("spl")
#endif
    {NULL, NULL, NULL}
};
#endif

zend_module_entry memcached_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
    STANDARD_MODULE_HEADER_EX, NULL,
    (zend_module_dep*)memcached_deps,
#else
    STANDARD_MODULE_HEADER,
#endif
	"memcached",
	NULL,
	PHP_MINIT(memcached),
	PHP_MSHUTDOWN(memcached),
	NULL,
	NULL,
	PHP_MINFO(memcached),
	PHP_MEMCACHED_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/* {{{ php_memc_register_constants */
static void php_memc_register_constants(INIT_FUNC_ARGS)
{
	#define REGISTER_MEMC_CLASS_CONST_LONG(name, value) zend_declare_class_constant_long(php_memc_get_ce() , ZEND_STRS( #name ) - 1, value TSRMLS_CC)
	#define REGISTER_MEMC_CLASS_CONST_BOOL(name, value) zend_declare_class_constant_bool(php_memc_get_ce() , ZEND_STRS( #name ) - 1, value TSRMLS_CC)
	#define REGISTER_MEMC_CLASS_CONST_NULL(name) zend_declare_class_constant_null(php_memc_get_ce() , ZEND_STRS( #name ) - 1)

	/*
	 * Class options
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(OPT_COMPRESSION, MEMC_OPT_COMPRESSION);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_PREFIX_KEY,  MEMC_OPT_PREFIX_KEY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERIALIZER,  MEMC_OPT_SERIALIZER);

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
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_LIBKETAMA_COMPATIBLE, MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_BUFFER_WRITES, MEMCACHED_BEHAVIOR_BUFFER_REQUESTS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_BINARY_PROTOCOL, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_NO_BLOCK, MEMCACHED_BEHAVIOR_NO_BLOCK);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_TCP_NODELAY, MEMCACHED_BEHAVIOR_TCP_NODELAY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SOCKET_SEND_SIZE, MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SOCKET_RECV_SIZE, MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_CONNECT_TIMEOUT, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RETRY_TIMEOUT, MEMCACHED_BEHAVIOR_RETRY_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SEND_TIMEOUT, MEMCACHED_BEHAVIOR_SND_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RECV_TIMEOUT, MEMCACHED_BEHAVIOR_RCV_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_POLL_TIMEOUT, MEMCACHED_BEHAVIOR_POLL_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_CACHE_LOOKUPS, MEMCACHED_BEHAVIOR_CACHE_LOOKUPS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERVER_FAILURE_LIMIT, MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_AUTO_EJECT_HOSTS, MEMCACHED_BEHAVIOR_AUTO_EJECT_HOSTS);

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
    REGISTER_MEMC_CLASS_CONST_LONG(RES_CONNECTION_SOCKET_CREATE_FAILURE, MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE);

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

	/*
	 * Flags.
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(GET_PRESERVE_ORDER, MEMC_GET_PRESERVE_ORDER);

	#undef REGISTER_MEMC_CLASS_CONST_LONG

	/*
	 * Return value from simple get errors
	 */
	REGISTER_MEMC_CLASS_CONST_NULL(GET_ERROR_RETURN_VALUE);
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{
	zend_class_entry ce;

    le_memc = zend_register_list_destructors_ex(NULL, php_memc_dtor, "Memcached persistent connection", module_number);

	INIT_CLASS_ENTRY(ce, "Memcached", memcached_class_methods);
	memcached_ce = zend_register_internal_class(&ce TSRMLS_CC);
	memcached_ce->create_object = php_memc_new;

    INIT_CLASS_ENTRY(ce, "MemcachedException", NULL);
	memcached_exception_ce = zend_register_internal_class_ex(&ce, php_memc_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);
	/* TODO
	 * possibly declare custom exception property here
	 */

	php_memc_register_constants(INIT_FUNC_ARGS_PASSTHRU);

#ifdef ZTS
	ts_allocate_id(&php_memcached_globals_id, sizeof(zend_php_memcached_globals),
				   (ts_allocate_ctor) php_memc_init_globals, (ts_allocate_dtor) php_memc_destroy_globals);
#else
	php_memc_init_globals(&php_memcached_globals TSRMLS_CC);
#endif

#if HAVE_MEMCACHED_SESSION
    php_session_register_module(ps_memcached_ptr);
#endif

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(memcached)
{
#ifdef ZTS
    ts_free_id(php_memcached_globals_id);
#else
    php_memc_destroy_globals(&php_memcached_globals TSRMLS_CC);
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

#if HAVE_MEMCACHED_SESSION
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
