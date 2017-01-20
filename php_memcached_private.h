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

#include <libmemcached/memcached.h>

#if defined(LIBMEMCACHED_VERSION_HEX) && LIBMEMCACHED_VERSION_HEX >= 0x01000017
typedef const memcached_instance_st * php_memcached_instance_st;
#else
typedef memcached_server_instance_st php_memcached_instance_st;
#endif

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
#include <ext/standard/php_smart_string.h>
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
typedef enum {
	SERIALIZER_PHP        = 1,
	SERIALIZER_IGBINARY   = 2,
	SERIALIZER_JSON       = 3,
	SERIALIZER_JSON_ARRAY = 4,
	SERIALIZER_MSGPACK    = 5
} php_memc_serializer_type;

typedef enum {
	COMPRESSION_TYPE_ZLIB   = 1,
	COMPRESSION_TYPE_FASTLZ = 2
} php_memc_compression_type;

typedef struct {
	const char *name;
	php_memc_serializer_type type;
} php_memc_serializer;

#if defined(HAVE_MEMCACHED_IGBINARY)
# define SERIALIZER_DEFAULT SERIALIZER_IGBINARY
# define SERIALIZER_DEFAULT_NAME "igbinary"
#elif defined(HAVE_MEMCACHED_MSGPACK)
# define SERIALIZER_DEFAULT SERIALIZER_MSGPACK
# define SERIALIZER_DEFAULT_NAME "msgpack"
#else
# define SERIALIZER_DEFAULT SERIALIZER_PHP
# define SERIALIZER_DEFAULT_NAME "php"
#endif /* HAVE_MEMCACHED_IGBINARY / HAVE_MEMCACHED_MSGPACK */

#ifdef HAVE_MEMCACHED_SASL
# include <sasl/sasl.h>
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
	/* Session related variables */
	struct {
		zend_bool lock_enabled;
		zend_long lock_wait_max;
		zend_long lock_wait_min;
		zend_long lock_retries;
		zend_long lock_expiration;

		zend_bool binary_protocol_enabled;
		zend_bool consistent_hash_enabled;

		zend_long server_failure_limit;
		zend_long number_of_replicas;
		zend_bool randomize_replica_read_enabled;
		zend_bool remove_failed_servers_enabled;

		zend_long connect_timeout;

		char *prefix;
		zend_bool persistent_enabled;

		char *sasl_username;
		char *sasl_password;
	} session;
#endif

	struct {
		char     *serializer_name;
		char     *compression_name;
		zend_long compression_threshold;
		double    compression_factor;
		zend_long store_retry_count;

		/* Converted values*/
		php_memc_serializer_type  serializer_type;
		php_memc_compression_type compression_type;

		/* Whether we have initialised sasl for this process */
		zend_bool sasl_initialised;

		struct {

			zend_bool consistent_hash_enabled;
			zend_bool binary_protocol_enabled;
			zend_long connect_timeout;

		} default_behavior;

	} memc;

	/* For deprecated values */
	zend_long no_effect;

#ifdef HAVE_MEMCACHED_PROTOCOL
	struct {
		php_memc_server_cb_t callbacks [MEMC_SERVER_ON_MAX];
	} server;
#endif

ZEND_END_MODULE_GLOBALS(php_memcached)

PHP_RINIT_FUNCTION(memcached);
PHP_RSHUTDOWN_FUNCTION(memcached);
PHP_MINIT_FUNCTION(memcached);
PHP_MSHUTDOWN_FUNCTION(memcached);
PHP_MINFO_FUNCTION(memcached);

char *php_memc_printable_func (zend_fcall_info *fci, zend_fcall_info_cache *fci_cache);

zend_bool php_memc_init_sasl_if_needed();

#endif /* PHP_MEMCACHED_PRIVATE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
