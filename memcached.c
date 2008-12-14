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

#include "php_memcached.h"

#if HAVE_MEMCACHED

/* {{{ memcached_functions[] */
function_entry memcached_functions[] = {
	{ NULL, NULL, NULL }
};
/* }}} */


/* {{{ memcached_module_entry
 */
zend_module_entry memcached_module_entry = {
	STANDARD_MODULE_HEADER,
	"memcached",
	memcached_functions,
	PHP_MINIT(memcached),     /* Replace with NULL if there is nothing to do at php startup   */ 
	PHP_MSHUTDOWN(memcached), /* Replace with NULL if there is nothing to do at php shutdown  */
	PHP_RINIT(memcached),     /* Replace with NULL if there is nothing to do at request start */
	PHP_RSHUTDOWN(memcached), /* Replace with NULL if there is nothing to do at request end   */
	PHP_MINFO(memcached),
	"0.1.0", 
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{

	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(memcached)
{

	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(memcached)
{
	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(memcached)
{
	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(memcached)
{
	php_info_print_box_start(0);
	php_printf("<p>libmemcached extension</p>\n");
	php_printf("<p>Version 0.1.0</p>\n");
	php_printf("<p><b>Authors:</b></p>\n");
	php_printf("<p>Andrei Zmievski &lt;andrei@php.net&gt; (lead)</p>\n");
	php_info_print_box_end();

}
/* }}} */

#endif /* HAVE_MEMCACHED */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: noet sw=4 ts=4 fdm=marker:
 */
