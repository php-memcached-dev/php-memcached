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
	const char *plist_key;
	int plist_key_len;
} php_memc_t;

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif

/****************************************
  Forward declarations
****************************************/
int php_memc_list_entry(void);


/****************************************
  Function implementations
****************************************/
static PHP_METHOD(Memcached, __construct)
{
	zval *object = getThis();
	php_memc_t *i_obj;
	char *persistent_id = NULL;
	int persistent_id_len;
	zend_bool skip_ctor = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &persistent_id,
							  &persistent_id_len) == FAILURE) {
		ZVAL_NULL(object);
		return;
	}

	i_obj = (php_memc_t *) zend_object_store_get_object(object TSRMLS_CC);

	if (persistent_id) {
		char *plist_key = NULL;
		int plist_key_len = 0;
		zend_rsrc_list_entry *le;
		php_memc_t *pi_obj = NULL;

		plist_key_len = spprintf(&plist_key, 0, "memcached:id=%s", persistent_id);
		if (zend_hash_find(&EG(persistent_list), plist_key, plist_key_len+1, (void *)&le) == SUCCESS) {
			if (le->type == php_memc_list_entry()) {
				pi_obj = (php_memc_t *) le->ptr;
			}
		}

		if (pi_obj) {
			skip_ctor = 1;
		} else {
			pi_obj = pecalloc(1, sizeof(*pi_obj), 1);

			if (pi_obj == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of memory: cannot allocate handle");
				/* not reached */
			}

			pi_obj->is_persistent = 1;
			if ((pi_obj->plist_key = pemalloc(plist_key_len + 1, 1)) == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of memory: cannot allocate handle");
				/* not reached */
			}
			memcpy((char *)pi_obj->plist_key, plist_key, plist_key_len + 1);
			pi_obj->plist_key_len = plist_key_len + 1;
		}

		pi_obj->ce = i_obj->ce;
		pi_obj->properties = i_obj->properties;
		efree(i_obj);
		i_obj = pi_obj;
		zend_object_store_set_object(object, i_obj TSRMLS_CC);

		if (plist_key) {
			efree(plist_key);
		}
	}

	if (skip_ctor) {
		return;
	}

	if (persistent_id) {
		zend_rsrc_list_entry le;

		le.type = php_memc_list_entry();
		le.ptr = i_obj;
		if (zend_hash_update(&EG(persistent_list), (char *)i_obj->plist_key,
							 i_obj->plist_key_len, (void *)&le, sizeof(le), NULL) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not register persistent entry");
			/* not reached */
		}
	}
}


/****************************************
  Internal support code
****************************************/
static void php_memc_free(php_memc_t *i_obj TSRMLS_DC)
{
	pefree(i_obj, i_obj->is_persistent);
}

static void php_memc_free_storage(php_memc_t *i_obj TSRMLS_DC)
{
    if (i_obj->properties) {
        zend_hash_destroy(i_obj->properties);
        efree(i_obj->properties);
        i_obj->properties = NULL;
    }

	if (!i_obj->is_persistent) {
		php_memc_free(i_obj TSRMLS_CC);
	}
}

zend_object_value php_memc_new(zend_class_entry *ce TSRMLS_DC)
{
    zend_object_value retval;
    php_memc_t *i_obj;
    zval *tmp;

    i_obj = emalloc(sizeof(*i_obj));
    memset(i_obj, 0, sizeof(*i_obj));
    i_obj->ce = ce;
    ALLOC_HASHTABLE(i_obj->properties);
    zend_hash_init(i_obj->properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(i_obj->properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(i_obj, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t)php_memc_free_storage, NULL TSRMLS_CC);
    retval.handlers = &std_object_handlers;

    return retval;
}

ZEND_RSRC_DTOR_FUNC(php_memc_dtor)
{
	FILE *fp;
	fp = fopen("/tmp/a.log", "a");
    if (rsrc->ptr) {
        php_memc_t *i_obj = (php_memc_t *)rsrc->ptr;
		fprintf(fp, "destroying list entry for '%s'\n", i_obj->plist_key);
		php_memc_free(i_obj TSRMLS_CC);
        rsrc->ptr = NULL;
    }
	fclose(fp);
}

int php_memc_list_entry(void)
{
	return le_memc;
}

/* {{{ memcached_functions[] */
static zend_function_entry memcached_functions[] = {
	{ NULL, NULL, NULL }
};
/* }}} */

/* {{{ memcached_class_methods */
static zend_function_entry memcached_class_methods[] = {
    PHP_ME(Memcached, __construct, NULL, ZEND_ACC_PUBLIC)
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


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{
	zend_class_entry ce;

    le_memc = zend_register_list_destructors_ex(NULL, php_memc_dtor, "Memcached persistent connection", module_number);

	INIT_CLASS_ENTRY(ce, "Memcached", memcached_class_methods);
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
