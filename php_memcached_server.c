/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2009-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Mikko Koppanen <mkoppanen@php.net>                          |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include "php_memcached.h"
#include "php_memcached_server.h"
#include "php_libmemcached_compat.h"

#include <event2/listener.h>

#undef NDEBUG
#undef _NDEBUG
#include <assert.h>

#define MEMC_GET_CB(cb_type) (MEMC_G(server_callbacks)[cb_type])
#define MEMC_HAS_CB(cb_type) (MEMC_GET_CB(cb_type).fci.size > 0)


ZEND_EXTERN_MODULE_GLOBALS(php_memcached)

struct _php_memc_proto_handler_t {
	memcached_binary_protocol_callback_st callbacks;
	struct memcached_protocol_st *protocol_handle;
	struct event_base *event_base;
};

typedef struct {
	struct memcached_protocol_client_st *protocol_client;
	struct event_base *event_base;
} php_memc_client_t;

static
long s_invoke_php_callback (php_memc_server_cb_t *cb, zval ***params, ssize_t param_count TSRMLS_DC)
{
	long retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval *retval_ptr = NULL;

	cb->fci.params      = params;
	cb->fci.param_count = param_count;

	/* Call the cb */
	cb->fci.no_separation  = 1;
	cb->fci.retval_ptr_ptr = &retval_ptr;

	if (zend_call_function(&(cb->fci), &(cb->fci_cache) TSRMLS_CC) == FAILURE) {
		php_error_docref (NULL TSRMLS_CC, E_WARNING, "Failed to invoke callback %s()", Z_STRVAL_P (cb->fci.function_name));
	}
	if (retval_ptr) {
		convert_to_long (retval_ptr);
		retval = Z_LVAL_P(retval_ptr);
		zval_ptr_dtor(&retval_ptr);
	}
	return retval;
}

// memcached protocol callbacks
static
protocol_binary_response_status s_add_handler(const void *cookie, const void *key, uint16_t key_len, const void *data,
                                              uint32_t data_len, uint32_t flags, uint32_t exptime, uint64_t *cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval *zkey, *zvalue, *zflags, *zexptime, *zcas;
	zval **params [5];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_ADD)) {
		return retval;
	}

	MAKE_STD_ZVAL(zkey);
	ZVAL_STRINGL(zkey, key, key_len, 1);

	MAKE_STD_ZVAL(zvalue);
	ZVAL_STRINGL(zvalue, data, data_len, 1);

	MAKE_STD_ZVAL(zflags);
	ZVAL_LONG(zflags, flags);

	MAKE_STD_ZVAL(zexptime);
	ZVAL_LONG(zexptime, exptime);

	MAKE_STD_ZVAL(zcas);
	ZVAL_NULL(zcas);

	params [0] = &zkey;
	params [1] = &zvalue;
	params [2] = &zflags;
	params [3] = &zexptime;
	params [4] = &zcas;

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_ADD), params, 5);

	if (Z_TYPE_P(zcas) != IS_NULL) {
		convert_to_double (zcas);
		*cas = (uint64_t) Z_DVAL_P(zcas);
	}

	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zvalue);
	zval_ptr_dtor (&zflags);
	zval_ptr_dtor (&zexptime);
	zval_ptr_dtor (&zcas);

	return retval;
}

static
protocol_binary_response_status s_append_handler(const void *cookie, const void *key, uint16_t key_len,
                                                 const void *data, uint32_t data_len, uint64_t cas, uint64_t *result_cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval *zkey, *zvalue, *zcas, *zresult_cas;
	zval **params [4];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_APPEND)) {
		return retval;
	}

	MAKE_STD_ZVAL(zkey);
	ZVAL_STRINGL(zkey, key, key_len, 1);

	MAKE_STD_ZVAL(zvalue);
	ZVAL_STRINGL(zvalue, data, data_len, 1);

	MAKE_STD_ZVAL(zcas);
	ZVAL_DOUBLE(zcas, cas);

	MAKE_STD_ZVAL(zresult_cas);
	ZVAL_NULL(zresult_cas);

	params [0] = &zkey;
	params [1] = &zvalue;
	params [2] = &zcas;
	params [3] = &zresult_cas;

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_APPEND), params, 4);

	if (Z_TYPE_P(zresult_cas) != IS_NULL) {
		convert_to_double (zresult_cas);
		*result_cas = (uint64_t) Z_DVAL_P(zresult_cas);
	}

	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zvalue);
	zval_ptr_dtor (&zcas);
	zval_ptr_dtor (&zresult_cas);

	return retval;
}

static
protocol_binary_response_status s_incr_decr_handler (php_memc_event_t event, const void *cookie, const void *key, uint16_t key_len, uint64_t delta,
                                                     uint64_t initial, uint32_t expiration, uint64_t *result, uint64_t *result_cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval *zkey, *zdelta, *zinital, *zexpiration, *zresult, *zresult_cas;
	zval **params [6];

	if (!MEMC_HAS_CB(event)) {
		return retval;
	}

	MAKE_STD_ZVAL(zkey);
	ZVAL_STRINGL(zkey, key, key_len, 1);

	MAKE_STD_ZVAL(zdelta);
	ZVAL_LONG(zdelta, (long) delta);

	MAKE_STD_ZVAL(zinital);
	ZVAL_LONG(zinital, (long) initial);

	MAKE_STD_ZVAL(zexpiration);
	ZVAL_LONG(zexpiration, (long) expiration);

	MAKE_STD_ZVAL(zresult);
	ZVAL_LONG(zresult, 0);

	MAKE_STD_ZVAL(zresult_cas);
	ZVAL_NULL(zresult_cas);

	params [0] = &zkey;
	params [1] = &zdelta;
	params [2] = &zinital;
	params [3] = &zexpiration;
	params [4] = &zresult;
	params [5] = &zresult_cas;

	retval = s_invoke_php_callback (&MEMC_GET_CB(event), params, 6);

	if (Z_TYPE_P(zresult) != IS_LONG) {
		convert_to_long (zresult);
	}
	*result = (uint64_t) Z_LVAL_P(zresult);

	if (Z_TYPE_P(zresult_cas) != IS_NULL) {
		convert_to_double (zresult_cas);
		*result_cas = (uint64_t) Z_DVAL_P(zresult_cas);
	}

	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zdelta);
	zval_ptr_dtor (&zinital);
	zval_ptr_dtor (&zexpiration);
	zval_ptr_dtor (&zresult);
	zval_ptr_dtor (&zresult_cas);

	return retval;
}

static
protocol_binary_response_status s_increment_handler (const void *cookie, const void *key, uint16_t key_len, uint64_t delta,
                                                     uint64_t initial, uint32_t expiration, uint64_t *result, uint64_t *result_cas)
{
	return
		s_incr_decr_handler (MEMC_SERVER_ON_INCREMENT, cookie, key, key_len, delta, initial, expiration, result, result_cas);
}

static
protocol_binary_response_status s_decrement_handler (const void *cookie, const void *key, uint16_t key_len, uint64_t delta,
                                                     uint64_t initial, uint32_t expiration, uint64_t *result, uint64_t *result_cas)
{
	return
		s_incr_decr_handler (MEMC_SERVER_ON_DECREMENT, cookie, key, key_len, delta, initial, expiration, result, result_cas);
}

static
protocol_binary_response_status s_delete_handler (const void *cookie, const void *key,
                                                  uint16_t key_len, uint64_t cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval *zkey, *zcas;
	zval **params [2];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_DELETE)) {
		return retval;
	}

	MAKE_STD_ZVAL(zkey);
	ZVAL_STRINGL(zkey, key, key_len, 1);

	MAKE_STD_ZVAL(zcas);
	ZVAL_DOUBLE(zcas, (double) cas);

	params [0] = &zkey;
	params [1] = &zcas;

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_DELETE), params, 2);

	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zcas);
	return retval;
}

static
protocol_binary_response_status s_flush_handler(const void *cookie, uint32_t when)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval *zwhen;
	zval **params [1];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_FLUSH)) {
		return retval;
	}

	MAKE_STD_ZVAL(zwhen);
	ZVAL_LONG(zwhen, (long) when);

	params [0] = &zwhen;
	retval     = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_FLUSH), params, 1);

	zval_ptr_dtor (&zwhen);
	return retval;
}

// libevent callbacks

static
void s_handle_memcached_event (evutil_socket_t fd, short what, void *arg)
{
	int rc;
	short flags = 0;
	php_memc_client_t *client = (php_memc_client_t *)arg;
	memcached_protocol_event_t events = memcached_protocol_client_work (client->protocol_client);

	if (events & MEMCACHED_PROTOCOL_ERROR_EVENT) {
		memcached_protocol_client_destroy (client->protocol_client);
		efree (client);
		evutil_closesocket (fd);
		return;
	}

	if (events & MEMCACHED_PROTOCOL_WRITE_EVENT) {
		flags = EV_WRITE;
	}

	if (events & MEMCACHED_PROTOCOL_READ_EVENT) {
		flags |= EV_READ;
	}

	rc = event_base_once (client->event_base, fd, flags, s_handle_memcached_event, client, NULL);
	if (rc != 0) {
		php_error_docref (NULL TSRMLS_CC, E_WARNING, "Failed to schedule events");
	}
}

static
void s_accept_cb (evutil_socket_t fd, short what, void *arg)
{
	int rc;
	php_memc_client_t *client;
	struct sockaddr_storage addr;
	socklen_t addr_len;
	evutil_socket_t sock;

	php_memc_proto_handler_t *handler = (php_memc_proto_handler_t *) arg;

	/* Accept the connection */
	addr_len = sizeof (addr);
	sock     = accept (fd, (struct sockaddr *) &addr, &addr_len);

	if (sock == -1) {
		php_error_docref (NULL TSRMLS_CC, E_WARNING, "Failed to accept the client: %s", strerror (errno));
		return;
	}

	client                  = ecalloc (1, sizeof (php_memc_client_t));
	client->protocol_client = memcached_protocol_create_client (handler->protocol_handle, sock);
	client->event_base      = handler->event_base;

	if (!client->protocol_client) {
		php_error_docref (NULL TSRMLS_CC, E_WARNING, "Failed to allocate protocol client");
		efree (client);
		evutil_closesocket (sock);
		return;
	}

	// TODO: this should timeout
	rc = event_base_once (handler->event_base, sock, EV_READ, s_handle_memcached_event, client, NULL);

	if (rc != 0) {
		php_error_docref (NULL TSRMLS_CC, E_WARNING, "Failed to add event for client");
		memcached_protocol_client_destroy (client->protocol_client);
		efree (client);
		evutil_closesocket (sock);
		return;
	}
}

php_memc_proto_handler_t *php_memc_proto_handler_new ()
{
	php_memc_proto_handler_t *handler = ecalloc (1, sizeof (php_memc_proto_handler_t));

	handler->protocol_handle = memcached_protocol_create_instance ();
	assert (handler->protocol_handle);

	memset (&handler->callbacks, 0, sizeof (memcached_binary_protocol_callback_st));

	handler->callbacks.interface_version          = MEMCACHED_PROTOCOL_HANDLER_V1;
	handler->callbacks.interface.v1.add           = s_add_handler;
	handler->callbacks.interface.v1.append        = s_append_handler;
	handler->callbacks.interface.v1.decrement     = s_decrement_handler;
	handler->callbacks.interface.v1.delete_object = s_delete_handler;
	handler->callbacks.interface.v1.flush_object  = s_flush_handler;
	/*
	handler->callbacks.interface.v1.get           = s_get_handler;
	*/
	handler->callbacks.interface.v1.increment     = s_increment_handler;
	/*
	handler->callbacks.interface.v1.noop          = s_noop_handler;
	handler->callbacks.interface.v1.prepend       = s_prepend_handler;
	handler->callbacks.interface.v1.quit          = s_quit_handler;
	handler->callbacks.interface.v1.replace       = s_replace_handler;
	handler->callbacks.interface.v1.set           = s_set_handler;
	handler->callbacks.interface.v1.stat          = s_stat_handler;
	handler->callbacks.interface.v1.version       = s_version_handler;
	*/

	memcached_binary_protocol_set_callbacks(handler->protocol_handle, &handler->callbacks);
	return handler;
}

static
evutil_socket_t s_create_listening_socket (const char *spec)
{
	evutil_socket_t sock;
	struct sockaddr_storage addr;
	int addr_len;

	int rc;

	addr_len = sizeof (struct sockaddr);
	rc = evutil_parse_sockaddr_port (spec, (struct sockaddr *) &addr, &addr_len);
	assert (rc == 0);

	sock = socket (AF_INET, SOCK_STREAM, 0);
	assert (sock >= 0);

	rc = bind (sock, (struct sockaddr *) &addr, addr_len);
	assert (sock >= 0);

	rc = listen (sock, 1024);
	assert (rc >= 0);

	rc = evutil_make_socket_nonblocking (sock);
	assert (rc == 0);

	rc = evutil_make_listen_socket_reuseable (sock);
	assert (rc == 0);

	rc = evutil_make_socket_closeonexec (sock);
	assert (rc == 0);

	return sock;
}

void php_memc_proto_handler_run (php_memc_proto_handler_t *handler)
{
	struct event *accept_event;
	evutil_socket_t sock = s_create_listening_socket ("127.0.0.1:3434");

	handler->event_base = event_base_new();

	accept_event = event_new (handler->event_base, sock, EV_READ | EV_PERSIST, s_accept_cb, handler);
	event_add (accept_event, NULL);

	int f = event_base_dispatch (handler->event_base);
	fprintf (stderr, "Re: %d\n", f);
}

void php_memc_proto_handler_destroy (php_memc_proto_handler_t **ptr)
{
	php_memc_proto_handler_t *handler = *ptr;

	if (handler->protocol_handle)
		memcached_protocol_destroy_instance (handler->protocol_handle);

	efree (handler);
	*ptr = NULL;
}