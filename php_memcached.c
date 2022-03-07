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

/* TODO
 * - set LIBKETAMA_COMPATIBLE as the default?
 * - fix unserialize(serialize($memc))
 */

#include "php_memcached.h"
#include "php_memcached_private.h"
#include "php_memcached_server.h"
#include "g_fmt.h"

#include <ctype.h>
#include <limits.h>

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
#ifdef PHP_WIN32
//Windows extensions are generally built together,
//so it wont be in the installed location
#include "igbinary.h"
#else
# include "ext/igbinary/igbinary.h"
#endif
#endif

#ifdef HAVE_MEMCACHED_MSGPACK
# include "ext/msgpack/php_msgpack.h"
#endif

# include "ext/spl/spl_exceptions.h"

static int le_memc;

static int php_memc_list_entry(void) {
	return le_memc;
}

/****************************************
  Protocol parameters
****************************************/
#define MEMC_OBJECT_KEY_MAX_LENGTH 250

/****************************************
  Custom options
****************************************/
#define MEMC_OPT_COMPRESSION        -1001
#define MEMC_OPT_PREFIX_KEY         -1002
#define MEMC_OPT_SERIALIZER         -1003
#define MEMC_OPT_COMPRESSION_TYPE   -1004
#define MEMC_OPT_STORE_RETRY_COUNT  -1005
#define MEMC_OPT_USER_FLAGS         -1006

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

#define MEMC_VAL_GET_FLAGS(internal_flags)               (((internal_flags) & MEMC_MASK_INTERNAL) >> 4)
#define MEMC_VAL_SET_FLAG(internal_flags, internal_flag) ((internal_flags) |= (((internal_flag) << 4) & MEMC_MASK_INTERNAL))
#define MEMC_VAL_HAS_FLAG(internal_flags, internal_flag) ((MEMC_VAL_GET_FLAGS(internal_flags) & (internal_flag)) == (internal_flag))
#define MEMC_VAL_DEL_FLAG(internal_flags, internal_flag) (internal_flags &= (~(((internal_flag) << 4) & MEMC_MASK_INTERNAL)))

/****************************************
  User-defined flags
****************************************/
#define MEMC_VAL_GET_USER_FLAGS(flags)            ((flags & MEMC_MASK_USER) >> 16)
#define MEMC_VAL_SET_USER_FLAGS(flags, udf_flags) ((flags) |= ((udf_flags << 16) & MEMC_MASK_USER))
#define MEMC_VAL_USER_FLAGS_MAX                   ((1 << 16) - 1)

/****************************************
  "get" operation flags
****************************************/
#define MEMC_GET_PRESERVE_ORDER 1
#define MEMC_GET_EXTENDED       2

/****************************************
  Helper macros
****************************************/
#define RETURN_FROM_GET RETURN_FALSE

/****************************************
  Structures and definitions
****************************************/

typedef enum {
	MEMC_OP_SET,
	MEMC_OP_TOUCH,
	MEMC_OP_ADD,
	MEMC_OP_REPLACE,
	MEMC_OP_APPEND,
	MEMC_OP_PREPEND
} php_memc_write_op;

typedef struct {

	zend_bool is_persistent;
	zend_bool compression_enabled;
	zend_bool encoding_enabled;

	zend_long serializer;
	zend_long compression_type;

	zend_long store_retry_count;
	zend_long set_udf_flags;

#ifdef HAVE_MEMCACHED_SASL
	zend_bool has_sasl_data;
#endif
} php_memc_user_data_t;

typedef struct {
	memcached_st *memc;
	zend_bool is_pristine;
	int rescode;
	int memc_errno;
	zend_object zo;
} php_memc_object_t;

typedef struct {
	size_t num_valid_keys;

	const char **mkeys;
	size_t *mkeys_len;

	zend_string **strings;

} php_memc_keys_t;

typedef struct {
	zval *object;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
} php_memc_result_callback_ctx_t;

static inline php_memc_object_t *php_memc_fetch_object(zend_object *obj) {
	return (php_memc_object_t *)((char *)obj - XtOffsetOf(php_memc_object_t, zo));
}
#define Z_MEMC_OBJ_P(zv) php_memc_fetch_object(Z_OBJ_P(zv));

#define MEMC_METHOD_INIT_VARS                          \
	zval*                  object         = getThis(); \
	php_memc_object_t*     intern         = NULL;      \
	php_memc_user_data_t*  memc_user_data = NULL;

#if PHP_VERSION_ID < 80000
#define MEMC_METHOD_FETCH_OBJECT                                                      \
	intern = Z_MEMC_OBJ_P(object);                                                    \
	if (!intern->memc) {                                                              \
		php_error_docref(NULL, E_WARNING, "Memcached constructor was not called");    \
		return;                                                                       \
	}                                                                                 \
	memc_user_data = (php_memc_user_data_t *) memcached_get_user_data(intern->memc);  \
	(void)memc_user_data; /* avoid unused variable warning */
#else
#define MEMC_METHOD_FETCH_OBJECT                                                      \
	intern = Z_MEMC_OBJ_P(object);                                                    \
	if (!intern->memc) {                                                              \
		zend_throw_error(NULL, "Memcached constructor was not called");               \
		RETURN_THROWS();                                                                       \
	}                                                                                 \
	memc_user_data = (php_memc_user_data_t *) memcached_get_user_data(intern->memc);  \
	(void)memc_user_data; /* avoid unused variable warning */
#endif

static
zend_bool s_memc_valid_key_binary(zend_string *key)
{
	return memchr(ZSTR_VAL(key), '\n', ZSTR_LEN(key)) == NULL;
}

static
zend_bool s_memc_valid_key_ascii(zend_string *key)
{
	const char *str = ZSTR_VAL(key);
	size_t i, len = ZSTR_LEN(key);

	for (i = 0; i < len; i++) {
		if (!isgraph(str[i]) || isspace(str[i]))
			return 0;
	}
	return 1;
}

#define MEMC_CHECK_KEY(intern, key)                                               \
	if (UNEXPECTED(ZSTR_LEN(key) == 0 ||                                          \
		ZSTR_LEN(key) > MEMC_OBJECT_KEY_MAX_LENGTH ||                             \
		(memcached_behavior_get(intern->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL) \
				? !s_memc_valid_key_binary(key)                                   \
				: !s_memc_valid_key_ascii(key)                                    \
		))) {                                                                     \
		intern->rescode = MEMCACHED_BAD_KEY_PROVIDED;                             \
		RETURN_FALSE;                                                             \
	}

#ifdef HAVE_MEMCACHED_PROTOCOL
typedef struct {
	php_memc_proto_handler_t *handler;
	zend_object zo;
} php_memc_server_t;

static inline php_memc_server_t *php_memc_server_fetch_object(zend_object *obj) {
	return (php_memc_server_t *)((char *)obj - XtOffsetOf(php_memc_server_t, zo));
}
#define Z_MEMC_SERVER_P(zv) php_memc_server_fetch_object(Z_OBJ_P(zv))

static zend_object_handlers memcached_server_object_handlers;
static zend_class_entry *memcached_server_ce = NULL;
#endif

static zend_class_entry *memcached_ce = NULL;
static zend_class_entry *memcached_exception_ce = NULL;
static zend_object_handlers memcached_object_handlers;

ZEND_DECLARE_MODULE_GLOBALS(php_memcached)

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif

static PHP_INI_MH(OnUpdateCompressionType)
{
	if (!new_value) {
		MEMC_G(compression_type) = COMPRESSION_TYPE_FASTLZ;
	} else if (!strcmp(ZSTR_VAL(new_value), "fastlz")) {
		MEMC_G(compression_type) = COMPRESSION_TYPE_FASTLZ;
	} else if (!strcmp(ZSTR_VAL(new_value), "zlib")) {
		MEMC_G(compression_type) = COMPRESSION_TYPE_ZLIB;
	} else {
		return FAILURE;
	}
	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}

static PHP_INI_MH(OnUpdateSerializer)
{
	if (!new_value) {
		MEMC_G(serializer_type) = SERIALIZER_DEFAULT;
	} else if (!strcmp(ZSTR_VAL(new_value), "php")) {
		MEMC_G(serializer_type) = SERIALIZER_PHP;
#ifdef HAVE_MEMCACHED_IGBINARY
	} else if (!strcmp(ZSTR_VAL(new_value), "igbinary")) {
		MEMC_G(serializer_type) = SERIALIZER_IGBINARY;
#endif // IGBINARY
#ifdef HAVE_JSON_API
	} else if (!strcmp(ZSTR_VAL(new_value), "json")) {
		MEMC_G(serializer_type) = SERIALIZER_JSON;
	} else if (!strcmp(ZSTR_VAL(new_value), "json_array")) {
		MEMC_G(serializer_type) = SERIALIZER_JSON_ARRAY;
#endif // JSON
#ifdef HAVE_MEMCACHED_MSGPACK
	} else if (!strcmp(ZSTR_VAL(new_value), "msgpack")) {
		MEMC_G(serializer_type) = SERIALIZER_MSGPACK;
#endif // msgpack
	} else {
		return FAILURE;
	}

	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}

#ifdef HAVE_MEMCACHED_SESSION
static
PHP_INI_MH(OnUpdateDeprecatedLockValue)
{
	if (ZSTR_LEN(new_value) > 0 && strcmp(ZSTR_VAL(new_value), "not set")) {
		php_error_docref(NULL, E_DEPRECATED, "memcached.sess_lock_wait and memcached.sess_lock_max_wait are deprecated. Please update your configuration to use memcached.sess_lock_wait_min, memcached.sess_lock_wait_max and memcached.sess_lock_retries");
	}
	return FAILURE;
}

static
PHP_INI_MH(OnUpdateSessionPrefixString)
{
	if (new_value && ZSTR_LEN(new_value) > 0) {
		if (ZSTR_LEN(new_value) > MEMCACHED_MAX_KEY) {
			php_error_docref(NULL, E_WARNING, "memcached.sess_prefix too long (max: %d)", MEMCACHED_MAX_KEY - 1);
			return FAILURE;
		}
		if (!s_memc_valid_key_ascii(new_value)) {
			php_error_docref(NULL, E_WARNING, "memcached.sess_prefix cannot contain whitespace or control characters");
			return FAILURE;
		}
	}
	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}

static
PHP_INI_MH(OnUpdateConsistentHash)
{
	if (!new_value) {
		MEMC_SESS_INI(consistent_hash_type) = MEMCACHED_BEHAVIOR_KETAMA;
	} else if (!strcmp(ZSTR_VAL(new_value), "ketama")) {
		MEMC_SESS_INI(consistent_hash_type) = MEMCACHED_BEHAVIOR_KETAMA;
	} else if (!strcmp(ZSTR_VAL(new_value), "ketama_weighted")) {
		MEMC_SESS_INI(consistent_hash_type) = MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED;
	} else {
		php_error_docref(NULL, E_WARNING, "memcached.sess_consistent_hash_type must be ketama or ketama_weighted");
		return FAILURE;
	}
	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}
#endif // HAVE_MEMCACHED_SESSION

#define MEMC_INI_ENTRY(key, default_value, update_fn, gkey) \
	STD_PHP_INI_ENTRY("memcached."key, default_value, PHP_INI_ALL, update_fn, memc.gkey, zend_php_memcached_globals, php_memcached_globals)

#define MEMC_INI_BOOL(key, default_value, update_fn, gkey) \
	STD_PHP_INI_BOOLEAN("memcached."key, default_value, PHP_INI_ALL, update_fn, memc.gkey, zend_php_memcached_globals, php_memcached_globals)

#define MEMC_INI_LINK(key, default_value, update_fn, gkey) \
	STD_PHP_INI_ENTRY_EX("memcached."key, default_value, PHP_INI_ALL, update_fn, memc.gkey, zend_php_memcached_globals, php_memcached_globals, display_link_numbers)

#define MEMC_SESSION_INI_ENTRY(key, default_value, update_fn, gkey) \
	STD_PHP_INI_ENTRY("memcached.sess_"key, default_value, PHP_INI_ALL, update_fn, session.gkey, zend_php_memcached_globals, php_memcached_globals)

#define MEMC_SESSION_INI_BOOL(key, default_value, update_fn, gkey) \
	STD_PHP_INI_BOOLEAN("memcached.sess_"key, default_value, PHP_INI_ALL, update_fn, session.gkey, zend_php_memcached_globals, php_memcached_globals)

#define MEMC_SESSION_INI_LINK(key, default_value, update_fn, gkey) \
	STD_PHP_INI_ENTRY_EX("memcached.sess_"key, default_value, PHP_INI_ALL, update_fn, session.gkey, zend_php_memcached_globals, php_memcached_globals, display_link_numbers)


/* {{{ INI entries */
PHP_INI_BEGIN()

#ifdef HAVE_MEMCACHED_SESSION
	MEMC_SESSION_INI_BOOL ("locking",                "1",          OnUpdateBool,           lock_enabled)
	MEMC_SESSION_INI_ENTRY("lock_wait_min",          "150",        OnUpdateLongGEZero,     lock_wait_min)
	MEMC_SESSION_INI_ENTRY("lock_wait_max",          "150",        OnUpdateLongGEZero,     lock_wait_max)
	MEMC_SESSION_INI_ENTRY("lock_retries",           "5",          OnUpdateLong,           lock_retries)
	MEMC_SESSION_INI_ENTRY("lock_expire",            "0",          OnUpdateLongGEZero,     lock_expiration)
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX < 0x01000018
	MEMC_SESSION_INI_BOOL ("binary_protocol",        "0",          OnUpdateBool,           binary_protocol_enabled)
#else
	MEMC_SESSION_INI_BOOL ("binary_protocol",        "1",          OnUpdateBool,           binary_protocol_enabled)
#endif
	MEMC_SESSION_INI_BOOL ("consistent_hash",        "1",          OnUpdateBool,           consistent_hash_enabled)
	MEMC_SESSION_INI_ENTRY("consistent_hash_type",   "ketama",     OnUpdateConsistentHash, consistent_hash_name)
	MEMC_SESSION_INI_ENTRY("number_of_replicas",     "0",          OnUpdateLongGEZero,     number_of_replicas)
	MEMC_SESSION_INI_BOOL ("randomize_replica_read", "0",          OnUpdateBool,           randomize_replica_read_enabled)
	MEMC_SESSION_INI_BOOL ("remove_failed_servers",  "0",          OnUpdateBool,           remove_failed_servers_enabled)
	MEMC_SESSION_INI_ENTRY("server_failure_limit",   "0",          OnUpdateLongGEZero,     server_failure_limit)
	MEMC_SESSION_INI_LINK ("connect_timeout",        "0",          OnUpdateLong,           connect_timeout)

	MEMC_SESSION_INI_ENTRY("sasl_username",          "",           OnUpdateString,         sasl_username)
	MEMC_SESSION_INI_ENTRY("sasl_password",          "",           OnUpdateString,         sasl_password)
	MEMC_SESSION_INI_BOOL ("persistent",             "0",          OnUpdateBool,           persistent_enabled)
	MEMC_SESSION_INI_ENTRY("prefix",                 "memc.sess.key.", OnUpdateSessionPrefixString,         prefix)
	
	/* Deprecated */
	STD_PHP_INI_ENTRY("memcached.sess_lock_wait", "not set", PHP_INI_ALL, OnUpdateDeprecatedLockValue, no_effect, zend_php_memcached_globals, php_memcached_globals)
	STD_PHP_INI_ENTRY("memcached.sess_lock_max_wait", "not set", PHP_INI_ALL, OnUpdateDeprecatedLockValue, no_effect, zend_php_memcached_globals, php_memcached_globals)
	
#endif

	MEMC_INI_ENTRY("compression_type",      "fastlz",                OnUpdateCompressionType, compression_name)
	MEMC_INI_ENTRY("compression_factor",    "1.3",                   OnUpdateReal,            compression_factor)
	MEMC_INI_ENTRY("compression_threshold", "2000",                  OnUpdateLong,            compression_threshold)
	MEMC_INI_ENTRY("serializer",            SERIALIZER_DEFAULT_NAME, OnUpdateSerializer,      serializer_name)
	MEMC_INI_ENTRY("store_retry_count",     "0",                     OnUpdateLong,            store_retry_count)

	MEMC_INI_BOOL ("default_consistent_hash",       "0", OnUpdateBool,       default_behavior.consistent_hash_enabled)
	MEMC_INI_BOOL ("default_binary_protocol",       "0", OnUpdateBool,       default_behavior.binary_protocol_enabled)
	MEMC_INI_LINK ("default_connect_timeout",       "0", OnUpdateLong,       default_behavior.connect_timeout)

PHP_INI_END()
/* }}} */

#undef MEMC_INI_BOOL
#undef MEMC_INI_LINK
#undef MEMC_INI_ENTRY
#undef MEMC_SESSION_INI_BOOL
#undef MEMC_SESSION_INI_LINK
#undef MEMC_SESSION_INI_ENTRY

/****************************************
  Forward declarations
****************************************/
static void php_memc_get_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_store_impl(INTERNAL_FUNCTION_PARAMETERS, int op, zend_bool by_key);
static void php_memc_setMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_delete_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);
static void php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key);

/* Invoke PHP functions */
static zend_bool s_invoke_cache_callback(zval *zobject, zend_fcall_info *fci, zend_fcall_info_cache *fcc, zend_bool with_cas, zend_string *key, zval *value);

/* Iterate result sets */
typedef zend_bool (*php_memc_result_apply_fn)(php_memc_object_t *intern, zend_string *key, zval *value, zval *cas, uint32_t flags, void *context);

static
memcached_return php_memc_result_apply(php_memc_object_t *intern, php_memc_result_apply_fn result_apply_fn, zend_bool fetch_delay, void *context);

static
zend_bool php_memc_mget_apply(php_memc_object_t *intern, zend_string *server_key,
								php_memc_keys_t *keys, php_memc_result_apply_fn result_apply_fn,
								zend_bool with_cas, void *context);


/* Callback functions for different server list iterations */
static
	memcached_return s_server_cursor_list_servers_cb(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context);

static
	memcached_return s_server_cursor_version_cb(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context);

static
	zend_bool s_memc_write_zval (php_memc_object_t *intern, php_memc_write_op op, zend_string *server_key, zend_string *key, zval *value, time_t expiration);

static
	void php_memc_destroy(memcached_st *memc, php_memc_user_data_t *memc_user_data);

static
	zend_bool s_memcached_result_to_zval(memcached_st *memc, memcached_result_st *result, zval *return_value);

static
	zend_string *s_zval_to_payload(php_memc_object_t *intern, zval *value, uint32_t *flags);

static
	void s_hash_to_keys(php_memc_keys_t *keys_out, HashTable *hash_in, zend_bool preserve_order, zval *return_value);

static
	void s_clear_keys(php_memc_keys_t *keys);


/****************************************
  Exported helper functions
****************************************/

zend_bool php_memc_init_sasl_if_needed()
{
#ifdef HAVE_MEMCACHED_SASL
	if (MEMC_G(sasl_initialised)) {
		return 1;
	}
	if (sasl_client_init(NULL) != SASL_OK) {
		php_error_docref(NULL, E_ERROR, "Failed to initialize SASL library");
		return 0;
	}
	return 1;
#else
	php_error_docref(NULL, E_ERROR, "Memcached not built with sasl support");
	return 0;
#endif
}

char *php_memc_printable_func (zend_fcall_info *fci, zend_fcall_info_cache *fci_cache)
{
	char *buffer = NULL;

	if (fci->object) {
		spprintf (&buffer, 0, "%s::%s", ZSTR_VAL(fci->object->ce->name), ZSTR_VAL(fci_cache->function_handler->common.function_name));
	} else {
		if (Z_TYPE (fci->function_name) == IS_OBJECT) {
			spprintf (&buffer, 0, "%s", ZSTR_VAL(Z_OBJCE(fci->function_name)->name));
		}
		else {
			spprintf (&buffer, 0, "%s", Z_STRVAL(fci->function_name));
		}
	}
	return buffer;
}


/****************************************
  Handling error codes
****************************************/

static
zend_bool s_memcached_return_is_error(memcached_return status, zend_bool strict)
{
	zend_bool result = 0;

	switch (status) {
		case MEMCACHED_SUCCESS:
		case MEMCACHED_STORED:
		case MEMCACHED_DELETED:
		case MEMCACHED_STAT:
		case MEMCACHED_END:
		case MEMCACHED_BUFFERED:
			result = 0;
			break;

		case MEMCACHED_SOME_ERRORS:
			result = strict;
			break;

		default:
			result = 1;
			break;
	}
	return result;
}

static
int s_memc_status_handle_result_code(php_memc_object_t *intern, memcached_return status)
{
	intern->rescode = status;
	intern->memc_errno = 0;

	if (s_memcached_return_is_error(status, 1)) {
		intern->memc_errno = memcached_last_error_errno(intern->memc);
		return FAILURE;
	}
	return SUCCESS;
}

static
void s_memc_set_status(php_memc_object_t *intern, memcached_return status, int memc_errno)
{
	intern->rescode = status;
	intern->memc_errno = memc_errno;
}

static
zend_bool s_memc_status_has_error(php_memc_object_t *intern)
{
	return s_memcached_return_is_error(intern->rescode, 1);
}

static
zend_bool s_memc_status_has_result_code(php_memc_object_t *intern, memcached_return status)
{
	return intern->rescode == status;
}

/****************************************
  Marshall cas tokens
****************************************/

static
void s_uint64_to_zval (zval *target, uint64_t value)
{
	if (value >= ((uint64_t) LONG_MAX)) {
		zend_string *buffer;
#ifdef PRIu64
		buffer = strpprintf (0, "%" PRIu64, value);
#else
		/* Best effort */
		buffer = strpprintf (0, "%llu", value);
#endif
		ZVAL_STR(target, buffer);
	}
	else {
		ZVAL_LONG (target, (zend_long) value);
	}
}

static
uint64_t s_zval_to_uint64 (zval *cas)
{
	switch (Z_TYPE_P (cas)) {
		case IS_LONG:
			return (uint64_t) zval_get_long (cas);
		break;

		case IS_DOUBLE:
			if (Z_DVAL_P (cas) < 0.0)
				return 0;

			return (uint64_t) zval_get_double (cas);
		break;

		case IS_STRING:
		{
			uint64_t val;
			char *end;

			errno = 0;
			val = (uint64_t) strtoull (Z_STRVAL_P (cas), &end, 0);

			if (*end || (errno == ERANGE && val == UINT64_MAX) || (errno != 0 && val == 0)) {
				php_error_docref(NULL, E_ERROR, "Failed to unmarshall cas token");
				return 0;
			}
			return val;
		}
		break;
	}
	return 0;
}


/****************************************
  Iterate over memcached results and mget
****************************************/

static
memcached_return php_memc_result_apply(php_memc_object_t *intern, php_memc_result_apply_fn result_apply_fn, zend_bool fetch_delay, void *context)
{
	memcached_result_st result, *result_ptr;
	memcached_return rc, status = MEMCACHED_SUCCESS;

	memcached_result_create(intern->memc, &result);

	do {
		result_ptr = memcached_fetch_result(intern->memc, &result, &rc);

		if (s_memcached_return_is_error(rc, 0)) {
			status = rc;
		}

		/* nothing returned */
		if (!result_ptr) {
			break;
		}
		else {
			zend_string *key;
			zval val, zcas;
			zend_bool retval;

			uint64_t cas;
			uint32_t flags;

			const char *res_key;
			size_t res_key_len;
			
			if (!s_memcached_result_to_zval(intern->memc, &result, &val)) {
				if (EG(exception)) {
					status = MEMC_RES_PAYLOAD_FAILURE;
					memcached_quit(intern->memc);
					break;
				}
				status = MEMCACHED_SOME_ERRORS;
				continue;
			}

			res_key     = memcached_result_key_value(&result);
			res_key_len = memcached_result_key_length(&result);
			cas         = memcached_result_cas(&result);
			flags       = memcached_result_flags(&result);

			s_uint64_to_zval(&zcas, cas);

			key = zend_string_init (res_key, res_key_len, 0);
			retval = result_apply_fn(intern, key, &val, &zcas, flags, context);

			zend_string_release(key);
			zval_ptr_dtor(&val);
			zval_ptr_dtor(&zcas);

			/* Stop iterating on false */
			if (!retval) {
				if (!fetch_delay) {
					/* Make sure we clear our results */
					while (memcached_fetch_result(intern->memc, &result, &rc)) {}
				}
				break;
			}
		}
	} while (result_ptr != NULL);

	memcached_result_free(&result);
	return status;
}

static
zend_bool php_memc_mget_apply(php_memc_object_t *intern, zend_string *server_key, php_memc_keys_t *keys,
						php_memc_result_apply_fn result_apply_fn, zend_bool with_cas, void *context)
{
	memcached_return status;
	int mget_status;
	uint64_t orig_cas_flag = 0;

	// Reset status code
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	if (!keys->num_valid_keys) {
		intern->rescode = MEMCACHED_BAD_KEY_PROVIDED;
		return 0;
	}

	if (with_cas) {
		orig_cas_flag = memcached_behavior_get (intern->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS);

		if (!orig_cas_flag) {
			memcached_behavior_set (intern->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);
		}
	}

	if (server_key) {
		status = memcached_mget_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), keys->mkeys, keys->mkeys_len, keys->num_valid_keys);
	} else {
		status = memcached_mget(intern->memc, keys->mkeys, keys->mkeys_len, keys->num_valid_keys);
	}

	/* Need to handle result code before restoring cas flags, would mess up errno */
	mget_status = s_memc_status_handle_result_code(intern, status);
		
	if (with_cas && !orig_cas_flag) {
		memcached_behavior_set (intern->memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, orig_cas_flag);
	}

	/* Return on failure codes */
	if (mget_status == FAILURE) {
		return 0;
	}

	if (!result_apply_fn) {
		/* no callback, for example getDelayed */
		return 1;
	}

	status = php_memc_result_apply(intern, result_apply_fn, 0, context);

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		return 0;
	}

	return 1;
}

/****************************************
  Invoke callbacks
****************************************/

static
zend_bool s_invoke_new_instance_cb(zval *object, zend_fcall_info *fci, zend_fcall_info_cache *fci_cache, zend_string *persistent_id)
{
	zend_bool ret = 1;
	zval retval;
	zval params[2];

	ZVAL_COPY(&params[0], object);
	if (persistent_id) {
		ZVAL_STR(&params[1], zend_string_copy(persistent_id));
	} else {
		ZVAL_NULL(&params[1]);
	}

	fci->retval = &retval;
	fci->params = params;
	fci->param_count = 2;

	if (zend_call_function(fci, fci_cache) == FAILURE) {
		char *buf = php_memc_printable_func (fci, fci_cache);
		php_error_docref(NULL, E_WARNING, "Failed to invoke 'on_new' callback %s()", buf);
		efree (buf);
		ret = 0;
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&retval);

	return ret;
}

static
zend_bool s_invoke_cache_callback(zval *zobject, zend_fcall_info *fci, zend_fcall_info_cache *fcc, zend_bool with_cas, zend_string *key, zval *value)
{
	zend_bool status = 0;
	zval params[4];
	zval retval;
	php_memc_object_t *intern = Z_MEMC_OBJ_P(zobject);

	/* Prepare params */
	ZVAL_COPY(&params[0], zobject);
	ZVAL_STR_COPY(&params[1], key);    /* key */
	ZVAL_NEW_REF(&params[2], value);   /* value */

	if (with_cas) {
		fci->param_count = 3;
	} else {
		ZVAL_NEW_EMPTY_REF(&params[3]);    /* expiration */
		ZVAL_NULL(Z_REFVAL(params[3]));
		fci->param_count = 4;
	}

	fci->retval = &retval;
	fci->params = params;

	if (zend_call_function(fci, fcc) == SUCCESS) {
		if (zend_is_true(&retval)) {
			time_t expiration;
			zval *val = Z_REFVAL(params[2]);

			if (with_cas) {
				if (Z_TYPE_P(val) == IS_ARRAY) {
					zval *rv = zend_hash_str_find(Z_ARRVAL_P(val), "value", sizeof("value") - 1);
					if (rv) {
						zval *cas = zend_hash_str_find(Z_ARRVAL_P(val), "cas", sizeof("cas") -1);
						expiration = cas? Z_LVAL_P(cas) : 0;
						status = s_memc_write_zval (intern, MEMC_OP_SET, NULL, key, rv, expiration);
					}
					/* memleak?  zval_ptr_dtor(value); */
					ZVAL_COPY(value, val);
				}
			} else {
				expiration = zval_get_long(Z_REFVAL(params[3]));
				status = s_memc_write_zval (intern, MEMC_OP_SET, NULL, key, val, expiration);
				/* memleak?  zval_ptr_dtor(value); */
				ZVAL_COPY(value, val);
			}
		}
	}
	else {
		s_memc_set_status(intern, MEMCACHED_NOTFOUND, 0);
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	if (!with_cas) {
		zval_ptr_dtor(&params[3]);
	}
	zval_ptr_dtor(&retval);

	return status;
}

/****************************************
  Wrapper for setting from zval
****************************************/

static
zend_bool s_compress_value (php_memc_compression_type compression_type, zend_string **payload_in, uint32_t *flags)
{
	/* status */
	zend_bool compress_status = 0;
	zend_string *payload = *payload_in;
	uint32_t compression_type_flag = 0;

	/* Additional 5% for the data */
	size_t buffer_size = (size_t) (((double) ZSTR_LEN(payload) * 1.05) + 1.0);
	char *buffer       = emalloc(buffer_size);

	/* Store compressed size here */
	size_t compressed_size = 0;
	uint32_t original_size = ZSTR_LEN(payload);

	switch (compression_type) {

		case COMPRESSION_TYPE_FASTLZ:
		{
			compressed_size = fastlz_compress(ZSTR_VAL(payload), ZSTR_LEN(payload), buffer);

			if (compressed_size > 0) {
				compress_status = 1;
				compression_type_flag = MEMC_VAL_COMPRESSION_FASTLZ;
			}
		}
			break;

		case COMPRESSION_TYPE_ZLIB:
		{
			compressed_size = buffer_size;
			int status = compress((Bytef *) buffer, &compressed_size, (Bytef *) ZSTR_VAL(payload), ZSTR_LEN(payload));

			if (status == Z_OK) {
				compress_status = 1;
				compression_type_flag = MEMC_VAL_COMPRESSION_ZLIB;
			}
		}
			break;

		default:
			compress_status = 0;
			break;
	}

	/* This means the value was too small to be compressed and ended up larger */
	if (ZSTR_LEN(payload) <= (compressed_size * MEMC_G(compression_factor))) {
		compress_status = 0;
	}

	/* Replace the payload with the compressed copy */
	if (compress_status) {
		MEMC_VAL_SET_FLAG(*flags, MEMC_VAL_COMPRESSED | compression_type_flag);
		payload = zend_string_realloc(payload, compressed_size + sizeof(uint32_t), 0);

		/* Copy the uin32_t at the beginning */
		memcpy(ZSTR_VAL(payload), &original_size, sizeof(uint32_t));
		memcpy(ZSTR_VAL(payload) + sizeof (uint32_t), buffer, compressed_size);
		efree(buffer);

		zend_string_forget_hash_val(payload);
		*payload_in = payload;

		return 1;
	}

	/* Original payload was not modified */
	efree(buffer);
	return 0;
}

static
zend_bool s_serialize_value (php_memc_serializer_type serializer, zval *value, smart_str *buf, uint32_t *flags)
{
	switch (serializer) {

		/*
			Igbinary serialization
		*/
#ifdef HAVE_MEMCACHED_IGBINARY
		case SERIALIZER_IGBINARY:
		{
			uint8_t *buffer;
			size_t buffer_len;

			if (igbinary_serialize(&buffer, &buffer_len, value) != 0) {
				php_error_docref(NULL, E_WARNING, "could not serialize value with igbinary");
				return 0;
			}
			smart_str_appendl (buf, (char *)buffer, buffer_len);
			efree(buffer);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_IGBINARY);
		}
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
zend_string *s_zval_to_payload(php_memc_object_t *intern, zval *value, uint32_t *flags)
{
	zend_string *payload;
	php_memc_user_data_t *memc_user_data = memcached_get_user_data(intern->memc);
	zend_bool should_compress = memc_user_data->compression_enabled;

	switch (Z_TYPE_P(value)) {

		case IS_STRING:
			payload = zval_get_string(value);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_STRING);
			break;

		case IS_LONG:
		{
			smart_str buffer = {0};
			smart_str_append_long (&buffer, Z_LVAL_P(value));
			smart_str_0(&buffer);
			payload = buffer.s;

			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_LONG);
			should_compress = 0;
		}
			break;

		case IS_DOUBLE:
		{
			char buffer[40];
			php_memcached_g_fmt(buffer, Z_DVAL_P(value));
			payload = zend_string_init (buffer, strlen (buffer), 0);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_DOUBLE);
			should_compress = 0;
		}
			break;

		case IS_TRUE:
			payload = zend_string_init ("1", 1, 0);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_BOOL);
			should_compress = 0;
			break;

		case IS_FALSE:
			payload = zend_string_alloc (0, 0);
			MEMC_VAL_SET_TYPE(*flags, MEMC_VAL_IS_BOOL);
			should_compress = 0;
			break;

		default:
		{
			smart_str buffer = {0};

			if (!s_serialize_value (memc_user_data->serializer, value, &buffer, flags)) {
				smart_str_free(&buffer);
				return NULL;
			}
			payload = buffer.s;
		}
			break;
	}

	/* turn off compression for values below the threshold */
	if (ZSTR_LEN(payload) == 0 || ZSTR_LEN(payload) < MEMC_G(compression_threshold)) {
		should_compress = 0;
	}

	/* If we have compression flag, compress the value */
	if (should_compress) {
		/* s_compress_value() will always leave a valid payload, even if that payload
		 * did not actually get compressed. The flags will be set according to the
		 * to the compression type or no compression.
		 *
		 * No need to check the return value because the payload is always valid.
		 */
		(void)s_compress_value (memc_user_data->compression_type, &payload, flags);
	}

	if (memc_user_data->set_udf_flags >= 0) {
		MEMC_VAL_SET_USER_FLAGS(*flags, ((uint32_t) memc_user_data->set_udf_flags));
	}

	return payload;
}

static
zend_bool s_should_retry_write (php_memc_object_t *intern, memcached_return status)
{
	if (memcached_server_count (intern->memc) == 0) {
		return 0;
	}

	return s_memcached_return_is_error (status, 1);
}

static
zend_bool s_memc_write_zval (php_memc_object_t *intern, php_memc_write_op op, zend_string *server_key, zend_string *key, zval *value, time_t expiration)
{
	uint32_t flags = 0;
	zend_string *payload = NULL;
	memcached_return status = 0;
	php_memc_user_data_t *memc_user_data = memcached_get_user_data(intern->memc);
	zend_long retries = memc_user_data->store_retry_count;

	if (value) {
		payload = s_zval_to_payload(intern, value, &flags);

		if (!payload) {
			s_memc_set_status(intern, MEMC_RES_PAYLOAD_FAILURE, 0);
			return 0;
		}
	}

#define memc_write_using_fn(fn_name) payload ? fn_name(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), ZSTR_VAL(payload), ZSTR_LEN(payload), expiration, flags) : MEMC_RES_PAYLOAD_FAILURE;
#define memc_write_using_fn_by_key(fn_name) payload ? fn_name(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), ZSTR_VAL(payload), ZSTR_LEN(payload), expiration, flags) : MEMC_RES_PAYLOAD_FAILURE;

	if (server_key) {
		switch (op) {
			case MEMC_OP_SET:
				status = memc_write_using_fn_by_key(memcached_set_by_key);
			break;

			case MEMC_OP_TOUCH:
				status = php_memcached_touch_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), expiration);
			break;
			
			case MEMC_OP_ADD:
				status = memc_write_using_fn_by_key(memcached_add_by_key);
			break;
			
			case MEMC_OP_REPLACE:
				status = memc_write_using_fn_by_key(memcached_replace_by_key);
			break;
			
			case MEMC_OP_APPEND:
				status = memc_write_using_fn_by_key(memcached_append_by_key);
			break;

			case MEMC_OP_PREPEND:
				status = memc_write_using_fn_by_key(memcached_prepend_by_key);
			break;
		}

		if (status == MEMCACHED_END) {
			status = MEMCACHED_SUCCESS;
		}
	}
	else {
retry:
		switch (op) {
			case MEMC_OP_SET:
				status = memc_write_using_fn(memcached_set);
			break;

			case MEMC_OP_TOUCH:
				status = php_memcached_touch(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), expiration);
			break;
			
			case MEMC_OP_ADD:
				status = memc_write_using_fn(memcached_add);
			break;
			
			case MEMC_OP_REPLACE:
				status = memc_write_using_fn(memcached_replace);
			break;
			
			case MEMC_OP_APPEND:
				status = memc_write_using_fn(memcached_append);
			break;

			case MEMC_OP_PREPEND:
				status = memc_write_using_fn(memcached_prepend);
			break;
		}
		if (status == MEMCACHED_END) {
			status = MEMCACHED_SUCCESS;
		}
	}

	if (s_should_retry_write (intern, status) && retries-- > 0) {
		goto retry;
	}

#undef memc_write_using_fn
#undef memc_write_using_fn_by_key

	if (payload) {
		zend_string_release(payload);
	}
	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		return 0;
	}
	return 1;
}


/****************************************
  Methods
****************************************/


/* {{{ Memcached::__construct([string persistent_id[, callback on_new[, string connection_str]]]))
   Creates a Memcached object, optionally using persistent memcache connection */
static PHP_METHOD(Memcached, __construct)
{
	php_memc_object_t *intern;
	php_memc_user_data_t *memc_user_data;

	zend_string *persistent_id = NULL;
	zend_string *conn_str = NULL;
	zend_string *plist_key = NULL;
	zend_fcall_info fci = {0};
	zend_fcall_info_cache fci_cache;

	zend_bool is_persistent = 0;

	/* "|S!f!S" */
	ZEND_PARSE_PARAMETERS_START(0, 3)
	        Z_PARAM_OPTIONAL
	        Z_PARAM_STR_EX(persistent_id, 1, 0)
	        Z_PARAM_FUNC_EX(fci, fci_cache, 1, 0)
	        Z_PARAM_STR(conn_str)
	ZEND_PARSE_PARAMETERS_END();

	intern = Z_MEMC_OBJ_P(getThis());
	intern->is_pristine = 1;

	if (persistent_id && persistent_id->len) {
		zend_resource *le;

		plist_key = zend_string_alloc(sizeof("memcached:id=") + persistent_id->len - 1, 0);
		snprintf(ZSTR_VAL(plist_key), plist_key->len + 1, "memcached:id=%s", ZSTR_VAL(persistent_id));

		if ((le = zend_hash_find_ptr(&EG(persistent_list), plist_key)) != NULL) {
			if (le->type == php_memc_list_entry()) {
				intern->memc = le->ptr;
				intern->is_pristine = 0;
				zend_string_release (plist_key);
				return;
			}
		}
		is_persistent = 1;
	}

	if (conn_str && conn_str->len > 0) {
		intern->memc = memcached (ZSTR_VAL(conn_str), ZSTR_LEN(conn_str));
	}
	else {
		intern->memc = memcached (NULL, 0);
	}

	if (!intern->memc) {
		php_error_docref(NULL, E_ERROR, "Failed to allocate memory for memcached structure");
		/* never reached */
	}

	memc_user_data                    = pecalloc (1, sizeof(*memc_user_data), is_persistent);
	memc_user_data->serializer        = MEMC_G(serializer_type);
	memc_user_data->compression_type  = MEMC_G(compression_type);
	memc_user_data->compression_enabled = 1;
	memc_user_data->encoding_enabled  = 0;
	memc_user_data->store_retry_count = MEMC_G(store_retry_count);
	memc_user_data->set_udf_flags     = -1;
	memc_user_data->is_persistent     = is_persistent;

	memcached_set_user_data(intern->memc, memc_user_data);

	/* Set default behaviors */
	if (MEMC_G(default_behavior.consistent_hash_enabled)) {
		memcached_return rc = memcached_behavior_set(intern->memc, MEMCACHED_BEHAVIOR_DISTRIBUTION, MEMCACHED_DISTRIBUTION_CONSISTENT);
		if (rc != MEMCACHED_SUCCESS) {
			php_error_docref(NULL, E_WARNING, "Failed to turn on consistent hash: %s", memcached_strerror(intern->memc, rc));
		}
	}

	if (MEMC_G(default_behavior.binary_protocol_enabled)) {
		memcached_return rc = memcached_behavior_set(intern->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
		if (rc != MEMCACHED_SUCCESS) {
			php_error_docref(NULL, E_WARNING, "Failed to turn on binary protocol: %s", memcached_strerror(intern->memc, rc));
		}
		/* Also enable TCP_NODELAY when binary protocol is enabled */
		rc = memcached_behavior_set(intern->memc, MEMCACHED_BEHAVIOR_TCP_NODELAY, 1);
		if (rc != MEMCACHED_SUCCESS) {
			php_error_docref(NULL, E_WARNING, "Failed to set TCP_NODELAY: %s", memcached_strerror(intern->memc, rc));
		}
	}

	if (MEMC_G(default_behavior.connect_timeout)) {
		memcached_return rc = memcached_behavior_set(intern->memc, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, MEMC_G(default_behavior.connect_timeout));
		if (rc != MEMCACHED_SUCCESS) {
			php_error_docref(NULL, E_WARNING, "Failed to set connect timeout: %s", memcached_strerror(intern->memc, rc));
		}
	}

	if (fci.size) {
		if (!s_invoke_new_instance_cb(getThis(), &fci, &fci_cache, persistent_id) || EG(exception)) {
			/* error calling or exception thrown from callback */
			if (plist_key) {
				zend_string_release(plist_key);
			}
			/*
				Setting intern->memc null prevents object destruction from freeing the memcached_st
				We free it manually here because it might be persistent and has not been
				registered to persistent_list yet
			*/
			php_memc_destroy(intern->memc, memc_user_data);
			intern->memc = NULL;
			return;
		}
	}

	if (plist_key) {
		zend_resource le;

		le.type = php_memc_list_entry();
		le.ptr  = intern->memc;

		GC_SET_REFCOUNT(&le, 1);

		/* plist_key is not a persistent allocated key, thus we use str_update here */
		if (zend_hash_str_update_mem(&EG(persistent_list), ZSTR_VAL(plist_key), ZSTR_LEN(plist_key), &le, sizeof(le)) == NULL) {
			zend_string_release(plist_key);
			php_error_docref(NULL, E_ERROR, "could not register persistent entry");
			/* not reached */
		}
		zend_string_release(plist_key);
	}
}
/* }}} */



static
void s_hash_to_keys(php_memc_keys_t *keys_out, HashTable *hash_in, zend_bool preserve_order, zval *return_value)
{
	size_t idx = 0, alloc_count;
	zval *zv;

	keys_out->num_valid_keys = 0;

	alloc_count = zend_hash_num_elements(hash_in);
	if (!alloc_count) {
		return;
	}
	keys_out->mkeys     = ecalloc (alloc_count, sizeof (char *));
	keys_out->mkeys_len = ecalloc (alloc_count, sizeof (size_t));
	keys_out->strings   = ecalloc (alloc_count, sizeof (zend_string *));

	ZEND_HASH_FOREACH_VAL(hash_in, zv) {
		zend_string *key = zval_get_string(zv);

		if (preserve_order && return_value) {
			add_assoc_null_ex(return_value, ZSTR_VAL(key), ZSTR_LEN(key));
		}

		if (ZSTR_LEN(key) > 0 && ZSTR_LEN(key) < MEMCACHED_MAX_KEY) {
			keys_out->mkeys[idx]     = ZSTR_VAL(key);
			keys_out->mkeys_len[idx] = ZSTR_LEN(key);

			keys_out->strings[idx] = key;
			idx++;
		}
		else {
			zend_string_release (key);
		}

	} ZEND_HASH_FOREACH_END();

	if (!idx) {
		efree (keys_out->mkeys);
		efree (keys_out->mkeys_len);
		efree (keys_out->strings);
	}
	keys_out->num_valid_keys = idx;
}

static
void s_key_to_keys(php_memc_keys_t *keys_out, zend_string *key)
{
	zval zv_keys;

	array_init(&zv_keys);
	add_next_index_str(&zv_keys, zend_string_copy(key));

	s_hash_to_keys(keys_out, Z_ARRVAL(zv_keys), 0, NULL);
	zval_ptr_dtor(&zv_keys);
}

static
void s_clear_keys(php_memc_keys_t *keys)
{
	size_t i;

	if (!keys->num_valid_keys) {
		return;
	}

	for (i = 0; i < keys->num_valid_keys; i++) {
		zend_string_release (keys->strings[i]);
	}
	efree(keys->strings);
	efree(keys->mkeys);
	efree(keys->mkeys_len);
}

typedef struct {
	zend_bool extended;
	zval *return_value;
} php_memc_get_ctx_t;

static
zend_bool s_get_apply_fn(php_memc_object_t *intern, zend_string *key, zval *value, zval *cas, uint32_t flags, void *in_context)
{
	php_memc_get_ctx_t *context = (php_memc_get_ctx_t *) in_context;

	if (context->extended) {
		Z_TRY_ADDREF_P(value);
		Z_TRY_ADDREF_P(cas);

		array_init (context->return_value);
		add_assoc_zval (context->return_value, "value", value);
		add_assoc_zval (context->return_value, "cas",   cas);
		add_assoc_long (context->return_value, "flags", (zend_long) MEMC_VAL_GET_USER_FLAGS(flags));
	}
	else {
		ZVAL_COPY(context->return_value, value);
	}
	return 0; /* Stop after one */
}

static
void php_memc_get_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	php_memc_get_ctx_t context = {0};
	php_memc_keys_t keys = {0};
	zend_long get_flags = 0;
	zend_string *key;
	zend_string *server_key = NULL;
	zend_bool mget_status;
	memcached_return status = MEMCACHED_SUCCESS;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		/* "SS|f!l" */
		ZEND_PARSE_PARAMETERS_START(2, 4)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_STR(key)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_FUNC_EX(fci, fcc, 1, 0)
		        Z_PARAM_LONG(get_flags)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "S|f!l" */
		ZEND_PARSE_PARAMETERS_START(1, 3)
		        Z_PARAM_STR(key)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_FUNC_EX(fci, fcc, 1, 0)
		        Z_PARAM_LONG(get_flags)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);
	MEMC_CHECK_KEY(intern, key);

	context.extended = (get_flags & MEMC_GET_EXTENDED);

	context.return_value = return_value;

	s_key_to_keys(&keys, key);
	mget_status = php_memc_mget_apply(intern, server_key, &keys, s_get_apply_fn, context.extended, &context);
	s_clear_keys(&keys);

	if (!mget_status) {
		if (s_memc_status_has_result_code(intern, MEMCACHED_NOTFOUND) && fci.size > 0) {
			status = s_invoke_cache_callback(object, &fci, &fcc, context.extended, key, return_value);

			if (!status) {
				zval_ptr_dtor(return_value);
				RETURN_FROM_GET;
			}
		}
	}

	if (s_memc_status_has_error(intern)) {
		zval_ptr_dtor(return_value);
		RETURN_FROM_GET;
	}
}

/* {{{ Memcached::get(string key [, mixed callback [, int get_flags = 0])
   Returns a value for the given key or false */
PHP_METHOD(Memcached, get)
{
	php_memc_get_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getByKey(string server_key, string key [, mixed callback [, int get_flags = 0])
   Returns a value for key from the server identified by the server key or false */
PHP_METHOD(Memcached, getByKey)
{
	php_memc_get_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

static
zend_bool s_get_multi_apply_fn(php_memc_object_t *intern, zend_string *key, zval *value, zval *cas, uint32_t flags, void *in_context)
{
	php_memc_get_ctx_t *context = (php_memc_get_ctx_t *) in_context;

	Z_TRY_ADDREF_P(value);

	if (context->extended) {
		zval node;

		Z_TRY_ADDREF_P(cas);

		array_init(&node);
		add_assoc_zval(&node, "value", value);
		add_assoc_zval(&node, "cas",   cas);
		add_assoc_long(&node, "flags", (zend_long) MEMC_VAL_GET_USER_FLAGS(flags));

		zend_symtable_update(Z_ARRVAL_P(context->return_value), key, &node);
	}
	else {
		zend_symtable_update(Z_ARRVAL_P(context->return_value), key, value);
	}
	return 1;
}

/* {{{ -- php_memc_getMulti_impl */
static void php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	php_memc_get_ctx_t context;
	php_memc_keys_t keys_out;

	zval *keys = NULL;
	zend_string *server_key = NULL;
	zend_long flags = 0;
	MEMC_METHOD_INIT_VARS;
	zend_bool retval, preserve_order;

	if (by_key) {
		/* "Sa|l" */
		ZEND_PARSE_PARAMETERS_START(2, 3)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_ARRAY(keys)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(flags)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "a|l" */
		ZEND_PARSE_PARAMETERS_START(1, 2)
		        Z_PARAM_ARRAY(keys)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(flags)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);
	if (zend_hash_num_elements(Z_ARRVAL_P(keys)) == 0) {
		/* BC compatible */
		s_memc_set_status(intern, MEMCACHED_NOTFOUND, 0);
		return;
	}

	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	preserve_order = (flags & MEMC_GET_PRESERVE_ORDER);
	s_hash_to_keys(&keys_out, Z_ARRVAL_P(keys), preserve_order, return_value);

	context.extended = (flags & MEMC_GET_EXTENDED);
	context.return_value = return_value;

	retval = php_memc_mget_apply(intern, server_key, &keys_out, s_get_multi_apply_fn, context.extended, &context);

	s_clear_keys(&keys_out);

	if (!retval && (s_memc_status_has_result_code(intern, MEMCACHED_NOTFOUND) || s_memc_status_has_result_code(intern, MEMCACHED_SOME_ERRORS))) {
		return;
	}

	if (!retval || EG(exception)) {
		zval_dtor(return_value);
		RETURN_FROM_GET;
	}
}
/* }}} */

/* {{{ Memcached::getMulti(array keys[, long flags = 0 ])
   Returns values for the given keys or false */
PHP_METHOD(Memcached, getMulti)
{
	php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Memcached::getMultiByKey(string server_key, array keys[, long flags = 0 ])
   Returns values for the given keys from the server identified by the server key or false */
PHP_METHOD(Memcached, getMultiByKey)
{
	php_memc_getMulti_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
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


static
void s_create_result_array(zend_string *key, zval *value, zval *cas, uint32_t flags, zval *return_value)
{
	Z_TRY_ADDREF_P(value);
	Z_TRY_ADDREF_P(cas);

	add_assoc_str_ex(return_value, ZEND_STRL("key"), zend_string_copy(key));
	add_assoc_zval_ex(return_value, ZEND_STRL("value"), value);

	if (Z_LVAL_P(cas)) {
		/* BC compatible */
		add_assoc_zval_ex(return_value, ZEND_STRL("cas"), cas);
		add_assoc_long_ex(return_value, ZEND_STRL("flags"), MEMC_VAL_GET_USER_FLAGS(flags));
	}
}

static
zend_bool s_result_callback_apply(php_memc_object_t *intern, zend_string *key, zval *value, zval *cas, uint32_t flags, void *in_context)
{
	zend_bool status = 1;
	zval params[2];
	zval retval;
	php_memc_result_callback_ctx_t *context = (php_memc_result_callback_ctx_t *) in_context;

	ZVAL_COPY(&params[0], context->object);
	array_init(&params[1]);

	s_create_result_array(key, value, cas, flags, &params[1]);

	context->fci.retval = &retval;
	context->fci.params = params;
	context->fci.param_count = 2;
	
	if (zend_call_function(&context->fci, &context->fcc) == FAILURE) {
		php_error_docref(NULL, E_WARNING, "could not invoke result callback");
		status = 0;
	}

	zval_ptr_dtor(&retval);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);

	return status;
}

/* {{{ -- php_memc_getDelayed_impl */
static void php_memc_getDelayed_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	php_memc_keys_t keys_out = {0};

	zval *keys = NULL;
	zend_string *server_key = NULL;
	zend_bool with_cas = 0;

	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;


	if (by_key) {
		/* "Sa/|bf!" */
		ZEND_PARSE_PARAMETERS_START(2, 4)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_ARRAY_EX(keys, 0, 1)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_BOOL(with_cas)
		        Z_PARAM_FUNC_EX(fci, fcc, 1, 0)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "a/|bf!" */
		ZEND_PARSE_PARAMETERS_START(1, 3)
		        Z_PARAM_ARRAY_EX(keys, 0, 1)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_BOOL(with_cas)
		        Z_PARAM_FUNC_EX(fci, fcc, 1, 0)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	s_hash_to_keys(&keys_out, Z_ARRVAL_P(keys), 0, NULL);

	if (fci.size > 0) {
		php_memc_result_callback_ctx_t context = {
			getThis(), fci, fcc
		};
		status = php_memc_mget_apply(intern, server_key, &keys_out, &s_result_callback_apply, with_cas, (void *) &context);
	}
	else {
		status = php_memc_mget_apply(intern, server_key, &keys_out, NULL, with_cas, NULL);
	}

	s_clear_keys(&keys_out);
	RETURN_BOOL(status);
}
/* }}} */

static
zend_bool s_fetch_apply(php_memc_object_t *intern, zend_string *key, zval *value, zval *cas, uint32_t flags, void *in_context)
{
	zval *return_value = (zval *) in_context;
	s_create_result_array(key, value, cas, flags, return_value);

	return 0; // stop iterating after one
}

/* {{{ Memcached::fetch()
   Returns the next result from a previous delayed request */
PHP_METHOD(Memcached, fetch)
{
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	array_init(return_value);
	status = php_memc_result_apply(intern, s_fetch_apply, 1, return_value);

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		zval_ptr_dtor(return_value);
		RETURN_FROM_GET;
	}
}
/* }}} */

static
zend_bool s_fetch_all_apply(php_memc_object_t *intern, zend_string *key, zval *value, zval *cas, uint32_t flags, void *in_context)
{
	zval zv;
	zval *return_value = (zval *) in_context;

	array_init (&zv);
	s_create_result_array(key, value, cas, flags, &zv);

	add_next_index_zval(return_value, &zv);
	return 1;
}

/* {{{ Memcached::fetchAll()
   Returns all the results from a previous delayed request */
PHP_METHOD(Memcached, fetchAll)
{
	memcached_return status = MEMCACHED_SUCCESS;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	array_init(return_value);
	status = php_memc_result_apply(intern, s_fetch_all_apply, 0, return_value);

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
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

/* {{{ Memcached::setMulti(array items [, int expiration  ])
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
	zend_string *server_key = NULL;
	zend_long expiration = 0;
	zval *value;
	zend_string *skey;
	zend_ulong num_key;
	int tmp_len = 0;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		/* "Sa|ll" */
		ZEND_PARSE_PARAMETERS_START(2, 3)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_ARRAY(entries)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "a|ll" */
		ZEND_PARSE_PARAMETERS_START(1, 2)
		        Z_PARAM_ARRAY(entries)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	ZEND_HASH_FOREACH_KEY_VAL (Z_ARRVAL_P(entries), num_key, skey, value) {
		zend_string *str_key = NULL;

		if (skey) {
			str_key = skey;
		}
		else {
			char tmp_key[64];

			tmp_len = snprintf(tmp_key, sizeof(tmp_key) - 1, "%ld", (long)num_key);
			str_key = zend_string_init(tmp_key, tmp_len, 0);
		}

		/* If this failed to write a value, intern stores the error for the return value */
		s_memc_write_zval (intern, MEMC_OP_SET, server_key, str_key, value, expiration);

		if (!skey) {
			zend_string_release (str_key);
		}

	} ZEND_HASH_FOREACH_END();

	RETURN_BOOL(!s_memc_status_has_error(intern));
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
	zend_string *key;
	zend_string *server_key = NULL;
	zend_string *s_value;
	zval s_zvalue;
	zval *value = NULL;
	zend_long expiration = 0;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			/* "SSS" */
			ZEND_PARSE_PARAMETERS_START(3, 3)
			        Z_PARAM_STR(server_key)
			        Z_PARAM_STR(key)
			        Z_PARAM_STR(s_value)
			ZEND_PARSE_PARAMETERS_END();
			value = &s_zvalue;
			ZVAL_STR(value, s_value);
		} else if (op == MEMC_OP_TOUCH) {
			/* "SS|l" */
			ZEND_PARSE_PARAMETERS_START(2, 3)
			        Z_PARAM_STR(server_key)
			        Z_PARAM_STR(key)
				Z_PARAM_OPTIONAL
				Z_PARAM_LONG(expiration)
			ZEND_PARSE_PARAMETERS_END();
		} else {
			/* "SSz|l" */
			ZEND_PARSE_PARAMETERS_START(3, 4)
			        Z_PARAM_STR(server_key)
			        Z_PARAM_STR(key)
			        Z_PARAM_ZVAL(value)
				Z_PARAM_OPTIONAL
				Z_PARAM_LONG(expiration)
			ZEND_PARSE_PARAMETERS_END();
		}
	} else {
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			/* "SS" */
			ZEND_PARSE_PARAMETERS_START(2, 2)
			        Z_PARAM_STR(key)
			        Z_PARAM_STR(s_value)
			ZEND_PARSE_PARAMETERS_END();
			value = &s_zvalue;
			ZVAL_STR(value, s_value);
		} else if (op == MEMC_OP_TOUCH) {
			/* "S|l */
			ZEND_PARSE_PARAMETERS_START(1, 2)
			        Z_PARAM_STR(key)
			        Z_PARAM_OPTIONAL
			        Z_PARAM_LONG(expiration)
			ZEND_PARSE_PARAMETERS_END();
		} else {
			/* "Sz|l" */
			ZEND_PARSE_PARAMETERS_START(2, 3)
			        Z_PARAM_STR(key)
			        Z_PARAM_ZVAL(value)
			        Z_PARAM_OPTIONAL
			        Z_PARAM_LONG(expiration)
			ZEND_PARSE_PARAMETERS_END();
		}
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);
	MEMC_CHECK_KEY(intern, key);

	if (memc_user_data->compression_enabled) {
		/*
		 * When compression is enabled, we cannot do appends/prepends because that would
		 * corrupt the compressed values. It is up to the user to fetch the value,
		 * append/prepend new data, and store it again.
		 */
		if (op == MEMC_OP_APPEND || op == MEMC_OP_PREPEND) {
			php_error_docref(NULL, E_WARNING, "cannot append/prepend with compression turned on");
			RETURN_NULL();
		}
	}

	if (!s_memc_write_zval (intern, op, server_key, key, value, expiration)) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ -- php_memc_cas_impl */
static void php_memc_cas_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *zv_cas;
	uint64_t cas;
	zend_string *key;
	zend_string *server_key = NULL;
	zval *value;
	zend_long expiration = 0;
	zend_string *payload;
	uint32_t flags = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		/* "zSSz|l" */
		ZEND_PARSE_PARAMETERS_START(4, 5)
		        Z_PARAM_ZVAL(zv_cas)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_STR(key)
		        Z_PARAM_ZVAL(value)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "zSz|l" */
		ZEND_PARSE_PARAMETERS_START(3, 4)
		        Z_PARAM_ZVAL(zv_cas)
		        Z_PARAM_STR(key)
		        Z_PARAM_ZVAL(value)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);
	MEMC_CHECK_KEY(intern, key);

	cas = s_zval_to_uint64(zv_cas);

	payload = s_zval_to_payload(intern, value, &flags);
	if (payload == NULL) {
		intern->rescode = MEMC_RES_PAYLOAD_FAILURE;
		RETURN_FALSE;
	}

	if (by_key) {
		status = memcached_cas_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), ZSTR_VAL(payload), ZSTR_LEN(payload), expiration, flags, cas);
	} else {
		status = memcached_cas(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), ZSTR_VAL(payload), ZSTR_LEN(payload), expiration, flags, cas);
	}

	zend_string_release(payload);
	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
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
	zend_long expiration = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		/* "SS|l" */
		ZEND_PARSE_PARAMETERS_START(2, 3)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_STR(key)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "S|l" */
		ZEND_PARSE_PARAMETERS_START(1, 2)
		        Z_PARAM_STR(key)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
		server_key = key;
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);
	MEMC_CHECK_KEY(intern, key);

	if (by_key) {
		status = memcached_delete_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key),
									 ZSTR_LEN(key), expiration);
	} else {
		status = memcached_delete(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), expiration);
	}

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ -- php_memc_deleteMulti_impl */
static void php_memc_deleteMulti_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key)
{
	zval *entries, *zv, ret;
	zend_string *server_key = NULL;
	zend_long expiration = 0;
	zend_string *entry;

	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	if (by_key) {
		/* "Sa/|l" */
		ZEND_PARSE_PARAMETERS_START(2, 3)
		        Z_PARAM_STR(server_key)
		        Z_PARAM_ARRAY_EX(entries, 0, 1)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "a/|l" */
		ZEND_PARSE_PARAMETERS_START(1, 2)
		        Z_PARAM_ARRAY_EX(entries, 0, 1)
		        Z_PARAM_OPTIONAL
		        Z_PARAM_LONG(expiration)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	array_init(return_value);
	ZEND_HASH_FOREACH_VAL (Z_ARRVAL_P(entries), zv) {
		entry = zval_get_string(zv);

		if (ZSTR_LEN(entry) == 0) {
			zend_string_release(entry);
			continue;
		}

		if (by_key) {
			status = memcached_delete_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(entry), ZSTR_LEN(entry), expiration);
		} else {
			status = memcached_delete_by_key(intern->memc, ZSTR_VAL(entry), ZSTR_LEN(entry), ZSTR_VAL(entry), ZSTR_LEN(entry), expiration);
		}

		if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
			ZVAL_LONG(&ret, status);
		} else {
			ZVAL_TRUE(&ret);
		}
		zend_symtable_update(Z_ARRVAL_P(return_value), entry, &ret);
		zend_string_release(entry);
	} ZEND_HASH_FOREACH_END();

	return;
}
/* }}} */

/* {{{ -- php_memc_incdec_impl */
static void php_memc_incdec_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool by_key, zend_bool incr)
{
	zend_string *key, *server_key = NULL;
	zend_long offset = 1;
	zend_long expiry = 0;
	zend_long initial = 0;
	uint64_t value = UINT64_MAX;
	memcached_return status;
	int n_args = ZEND_NUM_ARGS();

	MEMC_METHOD_INIT_VARS;

	if (!by_key) {
		/* "S|lll" */
		ZEND_PARSE_PARAMETERS_START(1, 4)
			Z_PARAM_STR(key)
			Z_PARAM_OPTIONAL
			Z_PARAM_LONG(offset)
			Z_PARAM_LONG(initial)
			Z_PARAM_LONG(expiry)
		ZEND_PARSE_PARAMETERS_END();
	} else {
		/* "SS|lll" */
		ZEND_PARSE_PARAMETERS_START(2, 5)
			Z_PARAM_STR(server_key)
			Z_PARAM_STR(key)
			Z_PARAM_OPTIONAL
			Z_PARAM_LONG(offset)
			Z_PARAM_LONG(initial)
			Z_PARAM_LONG(expiry)
		ZEND_PARSE_PARAMETERS_END();
	}

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);
	MEMC_CHECK_KEY(intern, key);

	if (offset < 0) {
		php_error_docref(NULL, E_WARNING, "offset cannot be a negative value");
		RETURN_FALSE;
	}

	if ((!by_key && n_args < 3) || (by_key && n_args < 4)) {
		if (by_key) {
			if (incr) {
				status = memcached_increment_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), offset, &value);
			} else {
				status = memcached_decrement_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), offset, &value);
			}
		} else {
			/* The libmemcached API has a quirk that memcached_increment() takes only a 32-bit
			 * offset, but memcached_increment_by_key() and all other increment and decrement
			 * functions take a 64-bit offset. The memcached protocol allows increment/decrement
			 * greater than UINT_MAX, so we just work around memcached_increment() here.
			 */
			if (incr) {
				status = memcached_increment_by_key(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), ZSTR_VAL(key), ZSTR_LEN(key), offset, &value);
			} else {
				status = memcached_decrement_by_key(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), ZSTR_VAL(key), ZSTR_LEN(key), offset, &value);
			}
		}

	} else {
		zend_long retries = memc_user_data->store_retry_count;

retry_inc_dec:
		if (!memcached_behavior_get(intern->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL)) {
			php_error_docref(NULL, E_WARNING, "Initial value is only supported with binary protocol");
			RETURN_FALSE;
		}
		if (by_key) {
			if (incr) {
				status = memcached_increment_with_initial_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), offset, initial, (time_t)expiry, &value);
			} else {
				status = memcached_decrement_with_initial_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), ZSTR_VAL(key), ZSTR_LEN(key), offset, initial, (time_t)expiry, &value);
			}
		} else {
			if (incr) {
				status = memcached_increment_with_initial(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), offset, initial, (time_t)expiry, &value);
			} else {
				status = memcached_decrement_with_initial(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key), offset, initial, (time_t)expiry, &value);
			}
		}
		if (s_should_retry_write(intern, status) && retries-- > 0) {
			goto retry_inc_dec;
		}
	}

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}

	if (value == UINT64_MAX) {
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
	zend_long port, weight = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	/* "Sa/|l" */
	ZEND_PARSE_PARAMETERS_START(2, 3)
	        Z_PARAM_STR(host)
	        Z_PARAM_LONG(port)
	        Z_PARAM_OPTIONAL
	        Z_PARAM_LONG(weight)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	status = memcached_server_add_with_weight(intern->memc, ZSTR_VAL(host), port, weight);

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
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
	HashPosition	pos;
	int   entry_size, i = 0;
	memcached_server_st *list = NULL;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	/* "a/" */
	ZEND_PARSE_PARAMETERS_START(1, 1)
	        Z_PARAM_ARRAY_EX(servers, 0, 1)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	ZEND_HASH_FOREACH_VAL (Z_ARRVAL_P(servers), entry) {
		if (Z_TYPE_P(entry) != IS_ARRAY) {
			php_error_docref(NULL, E_WARNING, "server list entry #%d is not an array", i+1);
			i++;
			continue;
		}

		entry_size = zend_hash_num_elements(Z_ARRVAL_P(entry));

		if (entry_size > 1) {
			zend_string *host;
			zend_long port;
			uint32_t weight;

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

			host = zval_get_string(z_host);
			port = zval_get_long(z_port);

			weight = 0;
			if (entry_size > 2) {
				/* Try to get weight */
				if (zend_hash_move_forward_ex(Z_ARRVAL_P(entry), &pos) == FAILURE ||
					(z_weight = zend_hash_get_current_data_ex(Z_ARRVAL_P(entry), &pos)) == NULL) {
					php_error_docref(NULL, E_WARNING, "could not get server weight for entry #%d", i+1);
				}

				weight = zval_get_long(z_weight);
			}

			list = memcached_server_list_append_with_weight(list, ZSTR_VAL(host), port, weight, &status);

			zend_string_release(host);

			if (s_memc_status_handle_result_code(intern, status) == SUCCESS) {
				i++;
				continue;
			}
		}
		i++;
		/* catch-all for all errors */
		php_error_docref(NULL, E_WARNING, "could not add entry #%d to the server list", i + 1);
	} ZEND_HASH_FOREACH_END();

	status = memcached_server_push(intern->memc, list);
	memcached_server_list_free(list);
	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::getServerList()
   Returns the list of the memcache servers in use */
PHP_METHOD(Memcached, getServerList)
{
	memcached_server_function callbacks[1];
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	callbacks[0] = s_server_cursor_list_servers_cb;
	array_init(return_value);
	memcached_server_cursor(intern->memc, callbacks, return_value, 1);
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

	/* "S" */
	ZEND_PARSE_PARAMETERS_START(1, 1)
	        Z_PARAM_STR(server_key)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	server_instance = memcached_server_by_key(intern->memc, ZSTR_VAL(server_key), ZSTR_LEN(server_key), &error);
	if (server_instance == NULL) {
		s_memc_status_handle_result_code(intern, error);
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

	memcached_servers_reset(intern->memc);
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

	memcached_quit(intern->memc);
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
	RETURN_BOOL(memcached_flush_buffers(intern->memc) == MEMCACHED_SUCCESS);
}
/* }}} */

/* {{{ Memcached::getLastErrorMessage()
   Returns the last error message that occurred */
PHP_METHOD(Memcached, getLastErrorMessage)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_STRING(memcached_last_error_message(intern->memc));
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

	RETURN_LONG(memcached_last_error(intern->memc));
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

	RETURN_LONG(memcached_last_error_errno(intern->memc));
}
/* }}} */

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

	server_instance = memcached_server_get_last_disconnect(intern->memc);
	if (server_instance == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string(return_value, "host", (char*) memcached_server_name(server_instance));
	add_assoc_long(return_value, "port", memcached_server_port(server_instance));
}
/* }}} */



static
zend_bool s_long_value(const char *str, zend_long *value)
{
	char *end = (char *) str;

	errno = 0;
	*value = strtol(str, &end, 10);

	if (errno || str == end || *end != '\0') {
		return 0;
	}
	return 1;
}

static
zend_bool s_double_value(const char *str, double *value)
{
	char *end = (char *) str;

	errno = 0;
	*value = strtod(str, &end);

	if (errno || str == end || *end != '\0') {
		return 0;
	}
	return 1;
}

static
memcached_return s_stat_execute_cb (php_memcached_instance_st instance, const char *key, size_t key_length, const char *value, size_t value_length, void *context)
{
	zend_string *server_key;
	zend_long long_val;
	double d_val;
	char *buffer;

	zval *return_value = (zval *) context;
	zval *server_values;

	server_key = strpprintf(0, "%s:%d", memcached_server_name(instance), memcached_server_port(instance));
	server_values = zend_hash_find(Z_ARRVAL_P(return_value), server_key);

	if (!server_values) {
		zval zv;
		array_init(&zv);

		server_values = zend_hash_add(Z_ARRVAL_P(return_value), server_key, &zv);
	}

	spprintf (&buffer, 0, "%.*s", (int)value_length, value);

	/* Check type */
	if (s_long_value (buffer, &long_val)) {
		add_assoc_long(server_values, key, long_val);
	}
	else if (s_double_value (buffer, &d_val)) {
		add_assoc_double(server_values, key, d_val);
	}
	else {
		add_assoc_stringl_ex(server_values, key, key_length, (char*)value, value_length);
	}
	efree (buffer);
	zend_string_release(server_key);

	return MEMCACHED_SUCCESS;
}

/* {{{ Memcached::getStats()
   Returns statistics for the memcache servers */
PHP_METHOD(Memcached, getStats)
{
	memcached_return status;
	char *args = NULL;
	zend_string *args_string = NULL;
	uint64_t orig_no_block, orig_protocol;
	MEMC_METHOD_INIT_VARS;

	/* "|S!" */
	ZEND_PARSE_PARAMETERS_START(0, 1)
	        Z_PARAM_OPTIONAL
	        Z_PARAM_STR_EX(args_string, 1, 0)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;

	if (args_string)
		args = ZSTR_VAL(args_string);

	/* stats hangs in nonblocking mode, turn off during the call. Only change the
	 * value if needed, because libmemcached reconnects for this behavior_set. */
	orig_no_block = memcached_behavior_get(intern->memc, MEMCACHED_BEHAVIOR_NO_BLOCK);
	orig_protocol = memcached_behavior_get(intern->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL);
	if (orig_no_block && orig_protocol)
		memcached_behavior_set(intern->memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 0);

	array_init(return_value);
	status = memcached_stat_execute(intern->memc, args, s_stat_execute_cb, return_value);

	if (orig_no_block && orig_protocol)
		memcached_behavior_set(intern->memc, MEMCACHED_BEHAVIOR_NO_BLOCK, orig_no_block);

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		zval_ptr_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::getVersion()
   Returns the version of each memcached server in the pool */
PHP_METHOD(Memcached, getVersion)
{
	memcached_return status;
	memcached_server_function callbacks[1];
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	status = memcached_version(intern->memc);
	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}

	callbacks[0] = s_server_cursor_version_cb;

	array_init(return_value);
	status = memcached_server_cursor(intern->memc, callbacks, return_value, 1);
	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::getAllKeys()
	Returns the keys stored on all the servers */
static
memcached_return s_dump_keys_cb(const memcached_st *ptr, const char *key, size_t key_length, void *in_context)
{
	zval *return_value = (zval*) in_context;
	add_next_index_stringl(return_value, key, key_length);

	return MEMCACHED_SUCCESS;
}

PHP_METHOD(Memcached, getAllKeys)
{
	memcached_return rc;
	memcached_dump_func callback[1];
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	callback[0] = s_dump_keys_cb;
	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);

	rc = memcached_dump(intern->memc, callback, return_value, 1);

	/* Ignore two errors. libmemcached has a hardcoded loop of 200 slab
	 * classes that matches memcached < 1.4.24, at which version the server
	 * has only 63 slabs and throws an error when requesting the 64th slab.
	 *
	 * In multi-server some non-deterministic number of elements will be dropped.
	 */
	if (rc != MEMCACHED_CLIENT_ERROR && rc != MEMCACHED_SERVER_ERROR
	    && s_memc_status_handle_result_code(intern, rc) == FAILURE) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Memcached::flush([ int delay ])
   Flushes the data on all the servers */
static PHP_METHOD(Memcached, flush)
{
	zend_long delay = 0;
	memcached_return status;
	MEMC_METHOD_INIT_VARS;

	/* "|l" */
	ZEND_PARSE_PARAMETERS_START(0, 1)
	        Z_PARAM_OPTIONAL
	        Z_PARAM_LONG(delay)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);

	status = memcached_flush(intern->memc, delay);
	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::getOption(int option)
   Returns the value for the given option constant */
static PHP_METHOD(Memcached, getOption)
{
	zend_long option;
	uint64_t result;
	memcached_behavior flag;
	MEMC_METHOD_INIT_VARS;

	/* "l" */
	ZEND_PARSE_PARAMETERS_START(1, 1)
	        Z_PARAM_LONG(option)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;

	switch (option) {
		case MEMC_OPT_COMPRESSION_TYPE:
			RETURN_LONG(memc_user_data->compression_type);

		case MEMC_OPT_COMPRESSION:
			RETURN_BOOL(memc_user_data->compression_enabled);

		case MEMC_OPT_PREFIX_KEY:
		{
			memcached_return retval;
			char *result;

			result = memcached_callback_get(intern->memc, MEMCACHED_CALLBACK_PREFIX_KEY, &retval);
			if (retval == MEMCACHED_SUCCESS && result) {
				RETURN_STRING(result);
			} else {
				RETURN_EMPTY_STRING();
			}
		}

		case MEMC_OPT_SERIALIZER:
			RETURN_LONG((long)memc_user_data->serializer);
			break;

		case MEMC_OPT_USER_FLAGS:
			RETURN_LONG(memc_user_data->set_udf_flags);
			break;

		case MEMC_OPT_STORE_RETRY_COUNT:
			RETURN_LONG((long)memc_user_data->store_retry_count);
			break;

		case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
		case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
			if (memcached_server_count(intern->memc) == 0) {
				php_error_docref(NULL, E_WARNING, "no servers defined");
				return;
			}

		default:
			/*
			 * Assume that it's a libmemcached behavior option.
			 */
			flag = (memcached_behavior) option;
			result = memcached_behavior_get(intern->memc, flag);
			RETURN_LONG((long)result);
	}
}
/* }}} */

static
int php_memc_set_option(php_memc_object_t *intern, long option, zval *value)
{
	zend_long lval;
	memcached_return rc = MEMCACHED_FAILURE;
	memcached_behavior flag;
	php_memc_user_data_t *memc_user_data = memcached_get_user_data(intern->memc);

	switch (option) {
		case MEMC_OPT_COMPRESSION:
			memc_user_data->compression_enabled = zval_get_long(value) ? 1 : 0;
			break;

		case MEMC_OPT_COMPRESSION_TYPE:
			lval = zval_get_long(value);
			if (lval == COMPRESSION_TYPE_FASTLZ ||
				lval == COMPRESSION_TYPE_ZLIB) {
				memc_user_data->compression_type = lval;
			} else {
				/* invalid compression type */
				intern->rescode = MEMCACHED_INVALID_ARGUMENTS;
				return 0;
			}
			break;

		case MEMC_OPT_PREFIX_KEY:
		{
			zend_string *str;
			char *key;
			str = zval_get_string(value);
			if (ZSTR_LEN(str) == 0) {
				key = NULL;
			} else {
				key = ZSTR_VAL(str);
			}
			if (memcached_callback_set(intern->memc, MEMCACHED_CALLBACK_PREFIX_KEY, key) == MEMCACHED_BAD_KEY_PROVIDED) {
				zend_string_release(str);
				intern->rescode = MEMCACHED_INVALID_ARGUMENTS;
				php_error_docref(NULL, E_WARNING, "bad key provided");
				return 0;
			}
			zend_string_release(str);
		}
			break;

		case MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED:
			flag = (memcached_behavior) option;

			lval = zval_get_long(value);
			rc = memcached_behavior_set(intern->memc, flag, (uint64_t)lval);

			if (s_memc_status_handle_result_code(intern, rc) == FAILURE) {
				php_error_docref(NULL, E_WARNING, "error setting memcached option: %s", memcached_strerror (intern->memc, rc));
				return 0;
			}

			/*
			 * This is necessary because libmemcached does not reset hash/distribution
			 * options on false case, like it does for MEMCACHED_BEHAVIOR_KETAMA
			 * (non-weighted) case. We have to clean up ourselves.
			 */
			if (!lval) {
				(void)memcached_behavior_set_key_hash(intern->memc, MEMCACHED_HASH_DEFAULT);
				(void)memcached_behavior_set_distribution_hash(intern->memc, MEMCACHED_HASH_DEFAULT);
				(void)memcached_behavior_set_distribution(intern->memc, MEMCACHED_DISTRIBUTION_MODULA);
			}
			break;

		case MEMC_OPT_SERIALIZER:
		{
			lval = zval_get_long(value);
			/* igbinary serializer */
#ifdef HAVE_MEMCACHED_IGBINARY
			if (lval == SERIALIZER_IGBINARY) {
				memc_user_data->serializer = SERIALIZER_IGBINARY;
			} else
#endif
#ifdef HAVE_JSON_API
			if (lval == SERIALIZER_JSON) {
				memc_user_data->serializer = SERIALIZER_JSON;
			} else if (lval == SERIALIZER_JSON_ARRAY) {
				memc_user_data->serializer = SERIALIZER_JSON_ARRAY;
			} else
#endif
			/* msgpack serializer */
#ifdef HAVE_MEMCACHED_MSGPACK
			if (lval == SERIALIZER_MSGPACK) {
				memc_user_data->serializer = SERIALIZER_MSGPACK;
			} else
#endif
			/* php serializer */
			if (lval == SERIALIZER_PHP) {
				memc_user_data->serializer = SERIALIZER_PHP;
			} else {
				memc_user_data->serializer = SERIALIZER_PHP;
				intern->rescode = MEMCACHED_INVALID_ARGUMENTS;
				php_error_docref(NULL, E_WARNING, "invalid serializer provided");
				return 0;
			}
			break;
		}

		case MEMC_OPT_USER_FLAGS:
			lval = zval_get_long(value);

			if (lval < 0) {
				memc_user_data->set_udf_flags = -1;
				return 1;
			}

			if (lval > MEMC_VAL_USER_FLAGS_MAX) {
				php_error_docref(NULL, E_WARNING, "MEMC_OPT_USER_FLAGS must be < %u", MEMC_VAL_USER_FLAGS_MAX);
				return 0;
			}
			memc_user_data->set_udf_flags = lval;
			break;

		case MEMC_OPT_STORE_RETRY_COUNT:
			lval = zval_get_long(value);
			memc_user_data->store_retry_count = lval;
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
				lval = zval_get_long(value);

				if (flag < MEMCACHED_BEHAVIOR_MAX) {
					// don't reset the option when the option value wasn't modified,
					// while the libmemcached may shutdown all connections.
					if (memcached_behavior_get(intern->memc, flag) == (uint64_t)lval) {
						return 1;
					}
					rc = memcached_behavior_set(intern->memc, flag, (uint64_t)lval);
				}
				else {
					rc = MEMCACHED_INVALID_ARGUMENTS;
				}
			}

			if (s_memc_status_handle_result_code(intern, rc) == FAILURE) {
				php_error_docref(NULL, E_WARNING, "error setting memcached option: %s", memcached_strerror (intern->memc, rc));
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
		zend_long value = 0;

		value = zval_get_long(pzval);
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
	zend_long replicas = 0;
	zend_bool retval = 1;

	uint32_t *server_map = NULL, *forward_map = NULL;
	size_t server_map_len = 0, forward_map_len = 0;
	memcached_return rc;
	MEMC_METHOD_INIT_VARS;

	/* "aa!l" */
	ZEND_PARSE_PARAMETERS_START(3, 3)
	        Z_PARAM_ARRAY(zserver_map)
	        Z_PARAM_ARRAY_EX(zforward_map, 1, 0)
	        Z_PARAM_LONG(replicas)
	ZEND_PARSE_PARAMETERS_END();

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

	rc = memcached_bucket_set (intern->memc, server_map, forward_map, (uint32_t) server_map_len, replicas);

	if (s_memc_status_handle_result_code(intern, rc) == FAILURE) {
		retval = 0;
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
	zend_ulong key_index;
	zval *value;

	MEMC_METHOD_INIT_VARS;

	/* "a" */
	ZEND_PARSE_PARAMETERS_START(1, 1)
	        Z_PARAM_ARRAY(options)
	ZEND_PARSE_PARAMETERS_END();


	MEMC_METHOD_FETCH_OBJECT;

	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), key_index, key, value) {
		if (key) {
			php_error_docref(NULL, E_WARNING, "invalid configuration option");
			ok = 0;
		} else {
			if (!php_memc_set_option(intern, (long) key_index, value)) {
				ok = 0;
			}
		}
	} ZEND_HASH_FOREACH_END();

	RETURN_BOOL(ok);
}
/* }}} */

/* {{{ Memcached::setOption(int option, mixed value)
   Sets the value for the given option constant */
static PHP_METHOD(Memcached, setOption)
{
	zend_long option;
	zval *value;
	MEMC_METHOD_INIT_VARS;

	/* "lz/" */
	ZEND_PARSE_PARAMETERS_START(2, 2)
	        Z_PARAM_LONG(option)
	        Z_PARAM_ZVAL_EX(value, 0, 1)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_BOOL(php_memc_set_option(intern, option, value));
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

	/* "SS/" */
	ZEND_PARSE_PARAMETERS_START(2, 2)
	        Z_PARAM_STR(user)
	        Z_PARAM_STR(pass)
	ZEND_PARSE_PARAMETERS_END();

	if (!php_memc_init_sasl_if_needed()) {
		RETURN_FALSE;
	}

	MEMC_METHOD_FETCH_OBJECT;

	if (!memcached_behavior_get(intern->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL)) {
		php_error_docref(NULL, E_WARNING, "SASL is only supported with binary protocol");
		RETURN_FALSE;
	}
	memc_user_data->has_sasl_data = 1;
	status = memcached_set_sasl_auth_data(intern->memc, ZSTR_VAL(user), ZSTR_VAL(pass));

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */
#endif /* HAVE_MEMCACHED_SASL */

#ifdef HAVE_MEMCACHED_SET_ENCODING_KEY
/* {{{ Memcached::setEncodingKey(string key)
   Sets AES encryption key (libmemcached 1.0.6 and higher) */
static PHP_METHOD(Memcached, setEncodingKey)
{
	MEMC_METHOD_INIT_VARS;
	memcached_return status;
	zend_string *key;

	/* "S" */
	ZEND_PARSE_PARAMETERS_START(1, 1)
	        Z_PARAM_STR(key)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;

	// libmemcached < 1.0.18 cannot handle a change of encoding key. Warn about this and return false.
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX < 0x01000018
	if (memc_user_data->encoding_enabled) {
		php_error_docref(NULL, E_WARNING, "libmemcached versions less than 1.0.18 cannot change encoding key");
		RETURN_FALSE;
	}
#endif

	status = memcached_set_encoding_key(intern->memc, ZSTR_VAL(key), ZSTR_LEN(key));

	if (s_memc_status_handle_result_code(intern, status) == FAILURE) {
		RETURN_FALSE;
	}

	memc_user_data->encoding_enabled = 1;
	RETURN_TRUE;
}
/* }}} */
#endif /* HAVE_MEMCACHED_SET_ENCODING_KEY */

/* {{{ Memcached::getResultCode()
   Returns the result code from the last operation */
static PHP_METHOD(Memcached, getResultCode)
{
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	RETURN_LONG(intern->rescode);
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

	switch (intern->rescode) {
		case MEMC_RES_PAYLOAD_FAILURE:
			RETURN_STRING("PAYLOAD FAILURE");
			break;

		case MEMCACHED_ERRNO:
		case MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE:
		case MEMCACHED_UNKNOWN_READ_FAILURE:
			if (intern->memc_errno) {
				zend_string *str = strpprintf(0, "%s: %s",
						memcached_strerror(intern->memc, (memcached_return)intern->rescode), strerror(intern->memc_errno));
				RETURN_STR(str);
			}
			/* Fall through */
		default:
			RETURN_STRING(memcached_strerror(intern->memc, (memcached_return)intern->rescode));
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

	RETURN_BOOL(memc_user_data->is_persistent);
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

	RETURN_BOOL(intern->is_pristine);
}
/* }}} */

/* {{{ bool Memcached::checkKey(string key)
   Checks if a key is valid */
PHP_METHOD(Memcached, checkKey)
{
	zend_string *key;
	MEMC_METHOD_INIT_VARS;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(key)
	ZEND_PARSE_PARAMETERS_END();

	MEMC_METHOD_FETCH_OBJECT;
	s_memc_set_status(intern, MEMCACHED_SUCCESS, 0);
	MEMC_CHECK_KEY(intern, key);
	RETURN_TRUE;
}
/* }}} */

/****************************************
  Internal support code
****************************************/

/* {{{ constructor/destructor */
static
void php_memc_destroy(memcached_st *memc, php_memc_user_data_t *memc_user_data)
{
#ifdef HAVE_MEMCACHED_SASL
	if (memc_user_data->has_sasl_data) {
		memcached_destroy_sasl_auth_data(memc);
	}
#endif

	memcached_free(memc);
	pefree(memc_user_data, memc_user_data->is_persistent);
}

static
void php_memc_object_free_storage(zend_object *object)
{
	php_memc_object_t *intern = php_memc_fetch_object(object);

	if (intern->memc) {
		php_memc_user_data_t *memc_user_data = memcached_get_user_data(intern->memc);

		if (!memc_user_data->is_persistent) {
			php_memc_destroy(intern->memc, memc_user_data);
		}
	}

	intern->memc = NULL;
	zend_object_std_dtor(&intern->zo);
}

static
zend_object *php_memc_object_new(zend_class_entry *ce)
{
	php_memc_object_t *intern = ecalloc(1, sizeof(php_memc_object_t) + zend_object_properties_size(ce));

	zend_object_std_init(&intern->zo, ce);
	object_properties_init(&intern->zo, ce);

	intern->zo.handlers = &memcached_object_handlers;
	return &intern->zo;
}

#ifdef HAVE_MEMCACHED_PROTOCOL
static
void php_memc_server_free_storage(zend_object *object)
{
	php_memc_server_t *intern = php_memc_server_fetch_object(object);

	php_memc_proto_handler_destroy(&intern->handler);
	zend_object_std_dtor(&intern->zo);
}

zend_object *php_memc_server_new(zend_class_entry *ce)
{
	php_memc_server_t *intern;

	intern = ecalloc(1, sizeof(php_memc_server_t) + zend_object_properties_size(ce));

	zend_object_std_init(&intern->zo, ce);
	object_properties_init(&intern->zo, ce);

	intern->zo.handlers = &memcached_server_object_handlers;
	intern->handler = php_memc_proto_handler_new();

	return &intern->zo;
}
#endif

ZEND_RSRC_DTOR_FUNC(php_memc_dtor)
{
	if (res->ptr) {
		memcached_st *memc = (memcached_st *) res->ptr;
		php_memc_destroy(memc, memcached_get_user_data(memc));
		res->ptr = NULL;
	}
}

/* }}} */

/* {{{ internal API functions */
static
memcached_return s_server_cursor_list_servers_cb(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context)
{
	zval array;
	zval *return_value = (zval *) in_context;

	array_init(&array);
	add_assoc_string(&array, "host", (char*)memcached_server_name(instance));
	add_assoc_long(&array,   "port", memcached_server_port(instance));
	add_assoc_string(&array, "type", (char*)memcached_server_type(instance));
	/*
	 * API does not allow to get at this field.
	add_assoc_long(array, "weight", instance->weight);
	*/

	add_next_index_zval(return_value, &array);
	return MEMCACHED_SUCCESS;
}

static
memcached_return s_server_cursor_version_cb(const memcached_st *ptr, php_memcached_instance_st instance, void *in_context)
{
	zend_string *address, *version;
	zval rv, *return_value = (zval *)in_context;

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000009
	version = strpprintf(0, "%d.%d.%d",
				memcached_server_major_version(instance),
				memcached_server_minor_version(instance),
				memcached_server_micro_version(instance));
#else
	version = strpprintf(0, "%d.%d.%d",
				instance->major_version,
				instance->minor_version,
				instance->micro_version);
#endif

	address = strpprintf(0, "%s:%d", memcached_server_name(instance), memcached_server_port(instance));

	ZVAL_STR(&rv, version);
	zend_hash_add(Z_ARRVAL_P(return_value), address, &rv);

	zend_string_release(address);

	return MEMCACHED_SUCCESS;
}


static
zend_string *s_decompress_value (const char *payload, size_t payload_len, uint32_t flags)
{
	zend_string *buffer;

	uint32_t stored_length;
	unsigned long length;
	zend_bool decompress_status = 0;
	zend_bool is_fastlz = 0, is_zlib = 0;

	if (payload_len < sizeof (uint32_t)) {
		return NULL;
	}

	is_fastlz = MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSION_FASTLZ);
	is_zlib   = MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSION_ZLIB);

	if (!is_fastlz && !is_zlib) {
		php_error_docref(NULL, E_WARNING, "could not decompress value: unrecognised compression type");
		return NULL;
	}

	memcpy(&stored_length, payload, sizeof (uint32_t));

	payload     += sizeof (uint32_t);
	payload_len -= sizeof (uint32_t);

	buffer = zend_string_alloc (stored_length, 0);

	if (is_fastlz) {
		decompress_status = ((length = fastlz_decompress(payload, payload_len, &buffer->val, buffer->len)) > 0);
	}
	else if (is_zlib) {
		decompress_status = (uncompress((Bytef *) buffer->val, &buffer->len, (Bytef *)payload, payload_len) == Z_OK);
	}

	ZSTR_VAL(buffer)[stored_length] = '\0';

	if (!decompress_status) {
		php_error_docref(NULL, E_WARNING, "could not decompress value");
		zend_string_release (buffer);
		return NULL;
	}

	zend_string_forget_hash_val(buffer);
	return buffer;
}

static
zend_bool s_unserialize_value (memcached_st *memc, int val_type, zend_string *payload, zval *return_value)
{
	switch (val_type) {
		case MEMC_VAL_IS_SERIALIZED:
		{
			php_unserialize_data_t var_hash;
			const unsigned char *p, *max;

			p   = (const unsigned char *) ZSTR_VAL(payload);
			max = p + ZSTR_LEN(payload);

			PHP_VAR_UNSERIALIZE_INIT(var_hash);
			if (!php_var_unserialize(return_value, &p, max, &var_hash)) {
				zval_ptr_dtor(return_value);
				ZVAL_FALSE(return_value);
				PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
				php_error_docref(NULL, E_WARNING, "could not unserialize value");
				return 0;
			}
			PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
		}
			break;

		case MEMC_VAL_IS_IGBINARY:
#ifdef HAVE_MEMCACHED_IGBINARY
			if (igbinary_unserialize((uint8_t *) ZSTR_VAL(payload), ZSTR_LEN(payload), return_value)) {
				ZVAL_FALSE(return_value);
				php_error_docref(NULL, E_WARNING, "could not unserialize value with igbinary");
				return 0;
			}
#else
			ZVAL_FALSE(return_value);
			php_error_docref(NULL, E_WARNING, "could not unserialize value, no igbinary support");
			return 0;
#endif
			break;

		case MEMC_VAL_IS_JSON:
#ifdef HAVE_JSON_API
		{
			php_memc_user_data_t *memc_user_data = memcached_get_user_data(memc);
			php_json_decode(return_value, ZSTR_VAL(payload), ZSTR_LEN(payload), (memc_user_data->serializer == SERIALIZER_JSON_ARRAY), PHP_JSON_PARSER_DEFAULT_DEPTH);
		}
#else
			ZVAL_FALSE(return_value);
			php_error_docref(NULL, E_WARNING, "could not unserialize value, no json support");
			return 0;
#endif
			break;

		case MEMC_VAL_IS_MSGPACK:
#ifdef HAVE_MEMCACHED_MSGPACK
			php_msgpack_unserialize(return_value, ZSTR_VAL(payload), ZSTR_LEN(payload));
#else
			ZVAL_FALSE(return_value);
			php_error_docref(NULL, E_WARNING, "could not unserialize value, no msgpack support");
			return 0;
#endif
 			break;
	}
	return 1;
}

static
zend_bool s_memcached_result_to_zval(memcached_st *memc, memcached_result_st *result, zval *return_value)
{
	zend_string *data;
	const char *payload;
	size_t payload_len;
	uint32_t flags;
	zend_bool retval = 1;

	payload     = memcached_result_value(result);
	payload_len = memcached_result_length(result);
	flags       = memcached_result_flags(result);

	if (!payload && payload_len > 0) {
		php_error_docref(NULL, E_WARNING, "Could not handle non-existing value of length %zu", payload_len);
		return 0;
	}

	if (MEMC_VAL_HAS_FLAG(flags, MEMC_VAL_COMPRESSED)) {
		data = s_decompress_value (payload, payload_len, flags);
		if (!data) {
			return 0;
		}
	} else {
		data = zend_string_init(payload, payload_len, 0);
	}

	switch (MEMC_VAL_GET_TYPE(flags)) {

		case MEMC_VAL_IS_STRING:
			ZVAL_STR_COPY(return_value, data);
			break;

		case MEMC_VAL_IS_LONG:
			ZVAL_LONG(return_value, strtol(ZSTR_VAL(data), NULL, 10));
			break;

		case MEMC_VAL_IS_DOUBLE:
		{
			if (zend_string_equals_literal(data, "Infinity")) {
				ZVAL_DOUBLE(return_value, php_get_inf());
			}
			else if (zend_string_equals_literal(data, "-Infinity")) {
				ZVAL_DOUBLE(return_value, -php_get_inf());
			}
			else if (zend_string_equals_literal(data, "NaN")) {
				ZVAL_DOUBLE(return_value, php_get_nan());
			}
			else {
				ZVAL_DOUBLE(return_value, zend_strtod(ZSTR_VAL(data), NULL));
			}
		}
			break;

		case MEMC_VAL_IS_BOOL:
			ZVAL_BOOL(return_value, payload_len > 0 && ZSTR_VAL(data)[0] == '1');
			break;

		case MEMC_VAL_IS_SERIALIZED:
		case MEMC_VAL_IS_IGBINARY:
		case MEMC_VAL_IS_JSON:
		case MEMC_VAL_IS_MSGPACK:
			retval = s_unserialize_value (memc, MEMC_VAL_GET_TYPE(flags), data, return_value);
			break;

		default:
			php_error_docref(NULL, E_WARNING, "unknown payload type");
			retval = 0;
			break;
	}
	zend_string_release(data);

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

	return zend_exception_get_default();
}


#ifdef HAVE_MEMCACHED_PROTOCOL

static
void s_destroy_cb (zend_fcall_info *fci)
{
	if (fci->size > 0) {
		zval_ptr_dtor(&fci->function_name);
		if (fci->object) {
			OBJ_RELEASE(fci->object);
		}
	}
}

static
PHP_METHOD(MemcachedServer, run)
{
	int i;
	zend_bool rc;
	zend_string *address;

	php_memc_server_t *intern;
	intern = Z_MEMC_SERVER_P(getThis());

	/* "S" */
	ZEND_PARSE_PARAMETERS_START(1, 1)
	        Z_PARAM_STR(address)
	ZEND_PARSE_PARAMETERS_END();

	rc = php_memc_proto_handler_run(intern->handler, address);

	for (i = MEMC_SERVER_ON_MIN + 1; i < MEMC_SERVER_ON_MAX; i++) {
		s_destroy_cb(&MEMC_SERVER_G(callbacks) [i].fci);
	}

	RETURN_BOOL(rc);
}

static
PHP_METHOD(MemcachedServer, on)
{
	zend_long event;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
	zend_bool rc = 0;

	/* "lf!" */
	ZEND_PARSE_PARAMETERS_START(2, 2)
	        Z_PARAM_LONG(event)
	        Z_PARAM_FUNC_EX(fci, fci_cache, 1, 0)
	ZEND_PARSE_PARAMETERS_END();

	if (event <= MEMC_SERVER_ON_MIN || event >= MEMC_SERVER_ON_MAX) {
		RETURN_FALSE;
	}

	if (fci.size > 0) {
		s_destroy_cb (&MEMC_SERVER_G(callbacks) [event].fci);

		MEMC_SERVER_G(callbacks) [event].fci       = fci;
		MEMC_SERVER_G(callbacks) [event].fci_cache = fci_cache;

		Z_TRY_ADDREF(fci.function_name);
		if (fci.object) {
			GC_ADDREF(fci.object);
		}
	}
	RETURN_BOOL(rc);
}

#endif

#if PHP_VERSION_ID < 80000
#include "php_memcached_legacy_arginfo.h"
#else
#include "php_memcached_arginfo.h"
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
	ZEND_MOD_REQUIRED("spl")
	ZEND_MOD_END
};
#endif

static
PHP_GINIT_FUNCTION(php_memcached)
{
#ifdef HAVE_MEMCACHED_SESSION
	php_memcached_globals->session.lock_enabled = 0;
	php_memcached_globals->session.lock_wait_max = 150;
	php_memcached_globals->session.lock_wait_min = 150;
	php_memcached_globals->session.lock_retries = 200;
	php_memcached_globals->session.lock_expiration = 30;
	php_memcached_globals->session.binary_protocol_enabled = 1;
	php_memcached_globals->session.consistent_hash_enabled = 1;
	php_memcached_globals->session.consistent_hash_type = MEMCACHED_BEHAVIOR_KETAMA;
	php_memcached_globals->session.consistent_hash_name = NULL;
	php_memcached_globals->session.number_of_replicas = 0;
	php_memcached_globals->session.server_failure_limit = 1;
	php_memcached_globals->session.randomize_replica_read_enabled = 1;
	php_memcached_globals->session.remove_failed_servers_enabled = 1;
	php_memcached_globals->session.connect_timeout = 1000;
	php_memcached_globals->session.prefix = NULL;
	php_memcached_globals->session.persistent_enabled = 0;
	php_memcached_globals->session.sasl_username = NULL;
	php_memcached_globals->session.sasl_password = NULL;
#endif

#ifdef HAVE_MEMCACHED_PROTOCOL
	memset(&php_memcached_globals->server, 0, sizeof(php_memcached_globals->server));
#endif

	php_memcached_globals->memc.serializer_name = NULL;
	php_memcached_globals->memc.serializer_type = SERIALIZER_DEFAULT;
	php_memcached_globals->memc.compression_name = NULL;
	php_memcached_globals->memc.compression_threshold = 2000;
	php_memcached_globals->memc.compression_type = COMPRESSION_TYPE_FASTLZ;
	php_memcached_globals->memc.compression_factor = 1.30;
	php_memcached_globals->memc.store_retry_count = 2;

	php_memcached_globals->memc.sasl_initialised = 0;
	php_memcached_globals->no_effect = 0;

	/* Defaults for certain options */
	php_memcached_globals->memc.default_behavior.consistent_hash_enabled = 0;
	php_memcached_globals->memc.default_behavior.binary_protocol_enabled = 0;
	php_memcached_globals->memc.default_behavior.connect_timeout         = 0;
}

zend_module_entry memcached_module_entry = {
	STANDARD_MODULE_HEADER_EX, NULL,
	memcached_deps,
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

	REGISTER_MEMC_CLASS_CONST_LONG(OPT_USER_FLAGS,  MEMC_OPT_USER_FLAGS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_STORE_RETRY_COUNT,  MEMC_OPT_STORE_RETRY_COUNT);

	/*
	 * Indicate whether igbinary serializer is available
	 */
#ifdef HAVE_MEMCACHED_IGBINARY
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_IGBINARY, 1);
#else
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_IGBINARY, 0);
#endif

	/*
	 * Indicate whether json serializer is available
	 */
#ifdef HAVE_JSON_API
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_JSON, 1);
#else
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_JSON, 0);
#endif

	/*
	 * Indicate whether msgpack serializer is available
	 */
#ifdef HAVE_MEMCACHED_MSGPACK
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_MSGPACK, 1);
#else
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_MSGPACK, 0);
#endif

	/*
	 * Indicate whether set_encoding_key is available
	 */
#ifdef HAVE_MEMCACHED_SET_ENCODING_KEY
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_ENCODING, 1);
#else
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_ENCODING, 0);
#endif

#ifdef HAVE_MEMCACHED_SESSION
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_SESSION, 1);
#else
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_SESSION, 0);
#endif

#ifdef HAVE_MEMCACHED_SASL
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_SASL, 1);
#else
	REGISTER_MEMC_CLASS_CONST_BOOL(HAVE_SASL, 0);
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
	REGISTER_MEMC_CLASS_CONST_LONG(DISTRIBUTION_VIRTUAL_BUCKET, MEMCACHED_DISTRIBUTION_VIRTUAL_BUCKET);
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
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_NUMBER_OF_REPLICAS, MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RANDOMIZE_REPLICA_READ, MEMCACHED_BEHAVIOR_RANDOMIZE_REPLICA_READ);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_REMOVE_FAILED_SERVERS, MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS);
#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000018
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERVER_TIMEOUT_LIMIT, MEMCACHED_BEHAVIOR_SERVER_TIMEOUT_LIMIT);
#endif

	/*
	 * libmemcached result codes
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(RES_SUCCESS,                   MEMCACHED_SUCCESS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_FAILURE,                   MEMCACHED_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_HOST_LOOKUP_FAILURE,       MEMCACHED_HOST_LOOKUP_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_CONNECTION_FAILURE,        MEMCACHED_CONNECTION_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_CONNECTION_BIND_FAILURE,   MEMCACHED_CONNECTION_BIND_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_WRITE_FAILURE,             MEMCACHED_WRITE_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_READ_FAILURE,              MEMCACHED_READ_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_UNKNOWN_READ_FAILURE,      MEMCACHED_UNKNOWN_READ_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_PROTOCOL_ERROR,            MEMCACHED_PROTOCOL_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_CLIENT_ERROR,              MEMCACHED_CLIENT_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_ERROR,              MEMCACHED_SERVER_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_DATA_EXISTS,               MEMCACHED_DATA_EXISTS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_DATA_DOES_NOT_EXIST,       MEMCACHED_DATA_DOES_NOT_EXIST);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NOTSTORED,                 MEMCACHED_NOTSTORED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_STORED,                    MEMCACHED_STORED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NOTFOUND,                  MEMCACHED_NOTFOUND);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_PARTIAL_READ,              MEMCACHED_PARTIAL_READ);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SOME_ERRORS,               MEMCACHED_SOME_ERRORS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NO_SERVERS,                MEMCACHED_NO_SERVERS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_END,                       MEMCACHED_END);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_DELETED,                   MEMCACHED_DELETED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_VALUE,                     MEMCACHED_VALUE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_STAT,                      MEMCACHED_STAT);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_ITEM,                      MEMCACHED_ITEM);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_ERRNO,                     MEMCACHED_ERRNO);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_FAIL_UNIX_SOCKET,          MEMCACHED_FAIL_UNIX_SOCKET);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NOT_SUPPORTED,             MEMCACHED_NOT_SUPPORTED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_NO_KEY_PROVIDED,           MEMCACHED_NO_KEY_PROVIDED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_FETCH_NOTFINISHED,         MEMCACHED_FETCH_NOTFINISHED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_TIMEOUT,                   MEMCACHED_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_BUFFERED,                  MEMCACHED_BUFFERED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_BAD_KEY_PROVIDED,          MEMCACHED_BAD_KEY_PROVIDED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_INVALID_HOST_PROTOCOL,     MEMCACHED_INVALID_HOST_PROTOCOL);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_MARKED_DEAD,        MEMCACHED_SERVER_MARKED_DEAD);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_UNKNOWN_STAT_KEY,          MEMCACHED_UNKNOWN_STAT_KEY);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_INVALID_ARGUMENTS,         MEMCACHED_INVALID_ARGUMENTS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_PARSE_ERROR,               MEMCACHED_PARSE_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_PARSE_USER_ERROR,          MEMCACHED_PARSE_USER_ERROR);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_DEPRECATED,                MEMCACHED_DEPRECATED);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_IN_PROGRESS,               MEMCACHED_IN_PROGRESS);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_MAXIMUM_RETURN,            MEMCACHED_MAXIMUM_RETURN);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_MEMORY_ALLOCATION_FAILURE, MEMCACHED_MEMORY_ALLOCATION_FAILURE);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_CONNECTION_SOCKET_CREATE_FAILURE, MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE);

	REGISTER_MEMC_CLASS_CONST_LONG(RES_E2BIG,                            MEMCACHED_E2BIG);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_KEY_TOO_BIG,                      MEMCACHED_KEY_TOO_BIG);
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_TEMPORARILY_DISABLED,      MEMCACHED_SERVER_TEMPORARILY_DISABLED);

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000008
	REGISTER_MEMC_CLASS_CONST_LONG(RES_SERVER_MEMORY_ALLOCATION_FAILURE, MEMCACHED_SERVER_MEMORY_ALLOCATION_FAILURE);
#endif

#ifdef HAVE_MEMCACHED_SASL
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
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_PHP,        SERIALIZER_PHP);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_IGBINARY,   SERIALIZER_IGBINARY);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_JSON,       SERIALIZER_JSON);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_JSON_ARRAY, SERIALIZER_JSON_ARRAY);
	REGISTER_MEMC_CLASS_CONST_LONG(SERIALIZER_MSGPACK,    SERIALIZER_MSGPACK);

	/*
	 * Compression types
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(COMPRESSION_FASTLZ, COMPRESSION_TYPE_FASTLZ);
	REGISTER_MEMC_CLASS_CONST_LONG(COMPRESSION_ZLIB,   COMPRESSION_TYPE_ZLIB);

	/*
	 * Flags.
	 */
	REGISTER_MEMC_CLASS_CONST_LONG(GET_PRESERVE_ORDER, MEMC_GET_PRESERVE_ORDER);
	REGISTER_MEMC_CLASS_CONST_LONG(GET_EXTENDED,       MEMC_GET_EXTENDED);

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

	/*
	 * Return value from simple get errors
	 */
	REGISTER_MEMC_CLASS_CONST_BOOL(GET_ERROR_RETURN_VALUE, 0);

	#undef REGISTER_MEMC_CLASS_CONST_LONG
	#undef REGISTER_MEMC_CLASS_CONST_BOOL
	#undef REGISTER_MEMC_CLASS_CONST_NULL

}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{
	zend_class_entry ce;

	memcpy(&memcached_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcached_object_handlers.offset    = XtOffsetOf(php_memc_object_t, zo);
	memcached_object_handlers.clone_obj = NULL;
	memcached_object_handlers.free_obj  = php_memc_object_free_storage;

	le_memc = zend_register_list_destructors_ex(NULL, php_memc_dtor, "Memcached persistent connection", module_number);

	INIT_CLASS_ENTRY(ce, "Memcached", class_Memcached_methods);
	memcached_ce = zend_register_internal_class(&ce);
	memcached_ce->create_object = php_memc_object_new;

#ifdef HAVE_MEMCACHED_PROTOCOL
	memcpy(&memcached_server_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcached_server_object_handlers.offset = XtOffsetOf(php_memc_server_t, zo);
	memcached_server_object_handlers.clone_obj = NULL;
	memcached_server_object_handlers.free_obj = php_memc_server_free_storage;

	INIT_CLASS_ENTRY(ce, "MemcachedServer", class_MemcachedServer_methods);
	memcached_server_ce = zend_register_internal_class(&ce);
	memcached_server_ce->create_object = php_memc_server_new;
#endif

	INIT_CLASS_ENTRY(ce, "MemcachedException", NULL);
	memcached_exception_ce = zend_register_internal_class_ex(&ce, php_memc_get_exception_base(0));
	/* TODO
	 * possibly declare custom exception property here
	 */

	php_memc_register_constants(INIT_FUNC_ARGS_PASSTHRU);
	REGISTER_INI_ENTRIES();

#ifdef HAVE_MEMCACHED_SESSION
	php_memc_session_minit(module_number);
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(memcached)
{
#ifdef HAVE_MEMCACHED_SASL
	if (MEMC_G(sasl_initialised)) {
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

#ifdef LIBMEMCACHED_AWESOME
	if (strcmp(LIBMEMCACHED_VERSION_STRING, memcached_lib_version())) {
		php_info_print_table_row(2, "libmemcached-awesome headers version", LIBMEMCACHED_VERSION_STRING);
		php_info_print_table_row(2, "libmemcached-awesome library version", memcached_lib_version());
	} else {
		php_info_print_table_row(2, "libmemcached-awesome version", memcached_lib_version());
	}
#else
	if (strcmp(LIBMEMCACHED_VERSION_STRING, memcached_lib_version())) {
		php_info_print_table_row(2, "libmemcached headers version", LIBMEMCACHED_VERSION_STRING);
		php_info_print_table_row(2, "libmemcached library version", memcached_lib_version());
	} else {
		php_info_print_table_row(2, "libmemcached version", memcached_lib_version());
	}
#endif

#ifdef HAVE_MEMCACHED_SASL
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
