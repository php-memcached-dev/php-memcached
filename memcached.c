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

/* TODO
 * - refcount on php_memc_t
 * - persistent_id in php_memc_t
 * - persistent switch-over in constructor
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>

#ifdef ZTS
#include "TSRM.h"
#endif

#include <php_ini.h>
#include <SAPI.h>
#include <ext/standard/info.h>
#include <Zend/zend_extensions.h>
#include <libmemcached/memcached.h>

#include "php_memcached.h"

static zend_class_entry *memcached_ce = NULL;
static int le_memc;

typedef struct {
	zend_class_entry *ce;
	HashTable *properties;
	unsigned int in_get:1;
	unsigned int in_set:1;

	memcached_st *conn;
	unsigned is_persistent:1;
} php_memc_t;

/* {{{ memcached_functions[] */
static zend_function_entry memcached_functions[] = {
	{ NULL, NULL, NULL }
};
/* }}} */

static zend_function_entry memcached_class_functions[] = {
	{ NULL, NULL, NULL }
};
/* }}} */

/* {{{ memcached_module_entry
 */
zend_module_entry memcached_module_entry = {
	STANDARD_MODULE_HEADER,
	"memcached",
	memcached_functions,
	PHP_MINIT(memcached),
	PHP_MSHUTDOWN(memcached),
	NULL,
	NULL,
	PHP_MINFO(memcached),
	PHP_MEMCACHED_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif

ZEND_RSRC_DTOR_FUNC(php_memc_dtor)
{
    if (rsrc->ptr) {
        php_memc_t *obj = (php_memc_t *)rsrc->ptr;
		/* TODO dtor code */
        rsrc->ptr = NULL;
    }
}

static void php_memc_free(php_memc_t *obj TSRMLS_DC)
{
	pefree(obj, obj->is_persistent);
}

static void php_memc_free_storage(php_memc_t *obj TSRMLS_DC)
{
    if (obj->properties) {
        zend_hash_destroy(obj->properties);
        efree(obj->properties);
        obj->properties = NULL;
    }

	php_memc_free(obj TSRMLS_CC);
}

zend_object_value php_memc_new(zend_class_entry *ce TSRMLS_DC)
{
    zend_object_value retval;
    php_memc_t *obj;
    zval *tmp;

    obj = emalloc(sizeof(*obj));
    memset(obj, 0, sizeof(*obj));
    obj->ce = ce;
    ALLOC_HASHTABLE(obj->properties);
    zend_hash_init(obj->properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(obj->properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(obj, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t)php_memc_free_storage, NULL TSRMLS_CC);
    retval.handlers = &std_object_handlers;

    return retval;
}


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{
	zend_class_entry ce;

    le_memc = zend_register_list_destructors_ex(NULL, php_memc_dtor, "Memcached persistent connection", module_number);

	INIT_CLASS_ENTRY(ce, "Memcached", memcached_class_functions);
	memcached_ce = zend_register_internal_class(&ce TSRMLS_CC);
	memcached_ce->create_object = php_memc_new;

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(memcached)
{

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(memcached)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "memcached support", "enabled");
	php_info_print_table_row(2, "Version", PHP_MEMCACHED_VERSION);
	php_info_print_table_row(2, "Revision", "$Revision: 1.107 $");
	php_info_print_table_end();
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: noet sw=4 ts=4 fdm=marker:
 */
