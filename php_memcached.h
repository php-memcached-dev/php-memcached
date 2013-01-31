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
};
#ifdef HAVE_MEMCACHED_IGBINARY
#define SERIALIZER_DEFAULT SERIALIZER_IGBINARY
#define SERIALIZER_DEFAULT_NAME "igbinary"
#else
#define SERIALIZER_DEFAULT SERIALIZER_PHP
#define SERIALIZER_DEFAULT_NAME "php"
#endif /* HAVE_MEMCACHED_IGBINARY */

#if LIBMEMCACHED_WITH_SASL_SUPPORT
# if defined(HAVE_SASL_SASL_H)
#  include <sasl/sasl.h>
#  define HAVE_MEMCACHED_SASL 1
# endif
#endif

ZEND_BEGIN_MODULE_GLOBALS(php_memcached)
#ifdef HAVE_MEMCACHED_SESSION
	zend_bool sess_locking_enabled;
	long  sess_lock_wait;
	char* sess_prefix;
	zend_bool sess_locked;
	char* sess_lock_key;
	int   sess_lock_key_len;

	int   sess_number_of_replicas;
	zend_bool sess_randomize_replica_read;
	zend_bool sess_remove_failed_enabled;
	zend_bool sess_consistent_hash_enabled;
	zend_bool sess_binary_enabled;
#endif
	char *serializer_name;
	enum memcached_serializer serializer;

	char *compression_type;
	int   compression_type_real;
	int   compression_threshold;

	double compression_factor;
#if HAVE_MEMCACHED_SASL
	bool use_sasl;
#endif
ZEND_END_MODULE_GLOBALS(php_memcached)

PHP_MEMCACHED_API zend_class_entry *php_memc_get_ce(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception_base(int root TSRMLS_DC);

PHP_RINIT_FUNCTION(memcached);
PHP_RSHUTDOWN_FUNCTION(memcached);
PHP_MINIT_FUNCTION(memcached);
PHP_MSHUTDOWN_FUNCTION(memcached);
PHP_MINFO_FUNCTION(memcached);

#define PHP_MEMCACHED_VERSION "2.1.0"

#ifdef ZTS
#define MEMC_G(v) TSRMG(php_memcached_globals_id, zend_php_memcached_globals *, v)
#else
#define MEMC_G(v) (php_memcached_globals.v)
#endif

typedef struct {
	memcached_st *memc_sess;
	zend_bool is_persisent;
} memcached_sess;

int php_memc_sess_list_entry(void);

#endif /* PHP_MEMCACHED_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
