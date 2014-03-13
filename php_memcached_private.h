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

#ifndef PHP_MEMCACHED_PRIVATE_H
#define PHP_MEMCACHED_PRIVATE_H

#include "main/php_config.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_libmemcached_compat.h"

#include <stdlib.h>
#include <string.h>
#include <php.h>
#include <php_main.h>

#ifdef ZTS
#  include "TSRM.h"
#endif

#include <php_ini.h>
#include <SAPI.h>
#include <ext/standard/info.h>
#include <zend_extensions.h>
#include <zend_exceptions.h>
#include <ext/standard/php_smart_str.h>
#include <ext/standard/php_var.h>
#include <ext/standard/basic_functions.h>

#ifdef PHP_WIN32
# include "win32/php_stdint.h"
#else
/* Used to store the size of the block */
#  if defined(HAVE_INTTYPES_H)
#    include <inttypes.h>
#  elif defined(HAVE_STDINT_H)
#    include <stdint.h>
#  endif
#endif

#ifndef HAVE_INT32_T
#  if SIZEOF_INT == 4
typedef int int32_t;
#  elif SIZEOF_LONG == 4
typedef long int int32_t;
#  endif
#endif

#ifndef HAVE_UINT32_T
#  if SIZEOF_INT == 4
typedef unsigned int uint32_t;
#  elif SIZEOF_LONG == 4
typedef unsigned long int uint32_t;
#  endif
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

#if HAVE_MEMCACHED_SASL
	char *sess_sasl_username;
	char *sess_sasl_password;
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
		php_memc_server_cb_t callbacks [MEMC_SERVER_ON_MAX];
	} server;
#endif
	long store_retry_count;
ZEND_END_MODULE_GLOBALS(php_memcached)

PHP_RINIT_FUNCTION(memcached);
PHP_RSHUTDOWN_FUNCTION(memcached);
PHP_MINIT_FUNCTION(memcached);
PHP_MSHUTDOWN_FUNCTION(memcached);
PHP_MINFO_FUNCTION(memcached);

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

#endif /* PHP_MEMCACHED_PRIVATE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
