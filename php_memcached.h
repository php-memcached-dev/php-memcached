/*
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

extern zend_module_entry memcached_module_entry;
#define phpext_memcached_ptr &memcached_module_entry

#ifdef PHP_WIN32
#define PHP_MEMCACHED_API __declspec(dllexport)
#else
#define PHP_MEMCACHED_API
#endif

PHP_MINIT_FUNCTION(memcached);
PHP_MSHUTDOWN_FUNCTION(memcached);
PHP_RINIT_FUNCTION(memcached);
PHP_RSHUTDOWN_FUNCTION(memcached);
PHP_MINFO_FUNCTION(memcached);

#define PHP_MEMCACHED_VERSION "0.1.0"

#endif /* PHP_MEMCACHED_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
