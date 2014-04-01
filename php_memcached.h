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

#include "php.h"
#include "main/php_config.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define PHP_MEMCACHED_VERSION "2.2.0"

#if defined(PHP_WIN32) && defined(MEMCACHED_EXPORTS)
#define PHP_MEMCACHED_API __declspec(dllexport)
#else
#define PHP_MEMCACHED_API PHPAPI
#endif

PHP_MEMCACHED_API zend_class_entry *php_memc_get_ce(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception(void);
PHP_MEMCACHED_API zend_class_entry *php_memc_get_exception_base(int root TSRMLS_DC);

extern zend_module_entry memcached_module_entry;
#define phpext_memcached_ptr &memcached_module_entry

#endif /* PHP_MEMCACHED_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
