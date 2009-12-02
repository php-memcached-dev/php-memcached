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

#include <libmemcached/memcached.h>

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

ZEND_BEGIN_MODULE_GLOBALS(php_memcached)
#if HAVE_MEMCACHED_SESSION
	zend_bool sess_locking_enabled;
	long  sess_lock_wait;
	char* sess_prefix;
	zend_bool sess_locked;
	char* sess_lock_key;
	int   sess_lock_key_len;
#endif
	enum memcached_serializer serializer;

	char *compression_type;
	int   compression_type_real;
	int   compression_threshold;

	double compression_factor;
ZEND_END_MODULE_GLOBALS(php_memcached)

PHP_MEMCACHED_API zend_class_entry *php_memc_get_ce(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception_base(int root TSRMLS_DC);

PHP_MINIT_FUNCTION(memcached);
PHP_MSHUTDOWN_FUNCTION(memcached);
PHP_MINFO_FUNCTION(memcached);

#define PHP_MEMCACHED_VERSION "1.0.0"

#ifdef ZTS
#define MEMC_G(v) TSRMG(php_memcached_globals_id, zend_php_memcached_globals *, v)
#else
#define MEMC_G(v) (php_memcached_globals.v)
#endif

/* session handler struct */
#if HAVE_MEMCACHED_SESSION
#include "ext/session/php_session.h"

extern ps_module ps_mod_memcached;
#define ps_memcached_ptr &ps_mod_memcached

PS_FUNCS(memcached);
#endif
#endif /* PHP_MEMCACHED_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
