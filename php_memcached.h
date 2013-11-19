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

#ifndef PHP_MEMCACHED_H
#define PHP_MEMCACHED_H

#include "php_libmemcached_compat.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

extern zend_module_entry memcached_module_entry;
#define phpext_memcached_ptr &memcached_module_entry

#ifdef PHP_WIN32
#define PHP_MEMCACHED_API __declspec(dllexport)
#else
#define PHP_MEMCACHED_API
#endif

/****************************************
  Structures and definitions
****************************************/
enum memcached_serializer {
	SERIALIZER_PHP = 1,
	SERIALIZER_IGBINARY = 2,
	SERIALIZER_JSON = 3,
	SERIALIZER_JSON_ARRAY = 4,
	SERIALIZER_MSGPACK = 5,
};
#ifdef HAVE_MEMCACHED_IGBINARY
#define SERIALIZER_DEFAULT SERIALIZER_IGBINARY
#define SERIALIZER_DEFAULT_NAME "igbinary"
#elif HAVE_MEMCACHED_MSGPACK
#define SERIALIZER_DEFAULT SERIALIZER_MSGPACK
#define SERIALIZER_DEFAULT_NAME "msgpack"
#else
#define SERIALIZER_DEFAULT SERIALIZER_PHP
#define SERIALIZER_DEFAULT_NAME "php"
#endif /* HAVE_MEMCACHED_IGBINARY / HAVE_MEMCACHED_MSGPACK */

#if LIBMEMCACHED_WITH_SASL_SUPPORT
# if defined(HAVE_SASL_SASL_H)
#  include <sasl/sasl.h>
#  define HAVE_MEMCACHED_SASL 1
# endif
#endif

#ifdef HAVE_MEMCACHED_PROTOCOL
typedef enum {
	MEMC_SERVER_ON_MIN       = -1,
	MEMC_SERVER_ON_CONNECT   = 0,
	MEMC_SERVER_ON_ADD       = 1,
	MEMC_SERVER_ON_APPEND    = 2,
	MEMC_SERVER_ON_DECREMENT = 3,
	MEMC_SERVER_ON_DELETE    = 4,
	MEMC_SERVER_ON_FLUSH     = 5,
	MEMC_SERVER_ON_GET       = 6,
	MEMC_SERVER_ON_INCREMENT = 7,
	MEMC_SERVER_ON_NOOP      = 8,
	MEMC_SERVER_ON_PREPEND   = 9,
	MEMC_SERVER_ON_QUIT      = 10,
	MEMC_SERVER_ON_REPLACE   = 11,
	MEMC_SERVER_ON_SET       = 12,
	MEMC_SERVER_ON_STAT      = 13,
	MEMC_SERVER_ON_VERSION   = 14,
	MEMC_SERVER_ON_MAX
} php_memc_event_t;


typedef struct {
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
} php_memc_server_cb_t;
#endif

ZEND_BEGIN_MODULE_GLOBALS(php_memcached)
#ifdef HAVE_MEMCACHED_SESSION
	zend_bool sess_locking_enabled;
	long  sess_lock_wait;
	long  sess_lock_max_wait;
	long  sess_lock_expire;
	char* sess_prefix;
	zend_bool sess_locked;
	char* sess_lock_key;
	int   sess_lock_key_len;

	int   sess_number_of_replicas;
	zend_bool sess_randomize_replica_read;
	zend_bool sess_remove_failed_enabled;
	long  sess_connect_timeout;
	zend_bool sess_consistent_hash_enabled;
	zend_bool sess_binary_enabled;

	char *sess_sasl_username;
	char *sess_sasl_password;
#if HAVE_MEMCACHED_SASL
	zend_bool sess_sasl_data;
#endif
#endif
	char *serializer_name;
	enum memcached_serializer serializer;

	char *compression_type;
	int   compression_type_real;
	int   compression_threshold;

	double compression_factor;
#if HAVE_MEMCACHED_SASL
	zend_bool use_sasl;
#endif
#ifdef HAVE_MEMCACHED_PROTOCOL
	struct {
		php_memc_server_cb_t callbacks [14];
	} server;
#endif
	long store_retry_count;
ZEND_END_MODULE_GLOBALS(php_memcached)

PHP_MEMCACHED_API zend_class_entry *php_memc_get_ce(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception_base(int root TSRMLS_DC);

PHP_RINIT_FUNCTION(memcached);
PHP_RSHUTDOWN_FUNCTION(memcached);
PHP_MINIT_FUNCTION(memcached);
PHP_MSHUTDOWN_FUNCTION(memcached);
PHP_MINFO_FUNCTION(memcached);

#define PHP_MEMCACHED_VERSION "2.2.0b1"

#ifdef ZTS
#define MEMC_G(v) TSRMG(php_memcached_globals_id, zend_php_memcached_globals *, v)
#else
#define MEMC_G(v) (php_memcached_globals.v)
#endif

typedef struct {
	memcached_st *memc_sess;
	zend_bool is_persistent;
} memcached_sess;

int php_memc_sess_list_entry(void);

char *php_memc_printable_func (zend_fcall_info *fci, zend_fcall_info_cache *fci_cache TSRMLS_DC);

#endif /* PHP_MEMCACHED_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
