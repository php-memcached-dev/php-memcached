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

#include "php_memcached.h"
#include "php_memcached_private.h"
#include "php_memcached_server.h"

#include <event2/listener.h>

#undef NDEBUG
#undef _NDEBUG
#include <assert.h>

#define MEMC_GET_CB(cb_type) (MEMC_SERVER_G(callbacks)[cb_type])
#define MEMC_HAS_CB(cb_type) (MEMC_GET_CB(cb_type).fci.size > 0)

#define MEMC_MAKE_ZVAL_COOKIE(my_zcookie, my_ptr) \
	do { \
		zend_string *cookie_buf; \
		cookie_buf = strpprintf(0, "%p", my_ptr); \
		ZVAL_STR(&my_zcookie, cookie_buf); \
	} while (0)

#define MEMC_MAKE_RESULT_CAS(my_zresult_cas, my_result_cas) \
	do { \
		my_result_cas = 0; \
		my_result_cas = zval_get_double(&my_zresult_cas); \
	} while (0)


ZEND_EXTERN_MODULE_GLOBALS(php_memcached)

struct _php_memc_proto_handler_t {
	memcached_binary_protocol_callback_st callbacks;
	struct memcached_protocol_st *protocol_handle;
	struct event_base *event_base;
};

typedef struct {
	struct memcached_protocol_client_st *protocol_client;
	struct event_base *event_base;
	zend_bool on_connect_invoked;
} php_memc_client_t;

static
long s_invoke_php_callback (php_memc_server_cb_t *cb, zval *params, ssize_t param_count)
{
	zval *retval = NULL;

	cb->fci.retval = retval;
	cb->fci.params = params;
	cb->fci.param_count = param_count;
	cb->fci.no_separation = 1;

	if (zend_call_function(&(cb->fci), &(cb->fci_cache)) == FAILURE) {
		char *buf = php_memc_printable_func(&(cb->fci), &(cb->fci_cache));
		php_error_docref(NULL, E_WARNING, "Failed to invoke callback %s()", buf);
		efree (buf);
	}

	return retval == NULL ? PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND : zval_get_long(retval);
}

// memcached protocol callbacks
static
protocol_binary_response_status s_add_handler(const void *cookie, const void *key, uint16_t key_len, const void *data,
                                              uint32_t data_len, uint32_t flags, uint32_t exptime, uint64_t *result_cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zkey, zvalue, zflags, zexptime, zresult_cas;
	zval params[6];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_ADD)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_STRINGL(&zkey, key, key_len);
	ZVAL_STRINGL(&zvalue, data, data_len);
	ZVAL_LONG(&zflags, flags);
	ZVAL_LONG(&zexptime, exptime);
	ZVAL_NULL(&zresult_cas);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zvalue);
	ZVAL_COPY(&params[3], &zflags);
	ZVAL_COPY(&params[4], &zexptime);
	ZVAL_COPY(&params[5], &zresult_cas);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_ADD), params, 6);

	MEMC_MAKE_RESULT_CAS(zresult_cas, *result_cas);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&params[3]);
	zval_ptr_dtor(&params[4]);
	zval_ptr_dtor(&params[5]);
	zval_ptr_dtor (&zcookie);
	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zvalue);
	zval_ptr_dtor (&zflags);
	zval_ptr_dtor (&zexptime);
	zval_ptr_dtor (&zresult_cas);

	return retval;
}

static
protocol_binary_response_status s_append_prepend_handler (php_memc_event_t event, const void *cookie, const void *key, uint16_t key_len,
                                                          const void *data, uint32_t data_len, uint64_t cas, uint64_t *result_cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zkey, zvalue, zcas, zresult_cas;
	zval params[5];

	if (!MEMC_HAS_CB(event)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_STRINGL(&zkey, key, key_len);
	ZVAL_STRINGL(&zvalue, data, data_len);
	ZVAL_DOUBLE(&zcas, cas);
	ZVAL_NULL(&zresult_cas);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zvalue);
	ZVAL_COPY(&params[3], &zcas);
	ZVAL_COPY(&params[4], &zresult_cas);

	retval = s_invoke_php_callback (&MEMC_GET_CB(event), params, 5);

	MEMC_MAKE_RESULT_CAS(zresult_cas, *result_cas);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&params[3]);
	zval_ptr_dtor(&params[4]);
	zval_ptr_dtor (&zcookie);
	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zvalue);
	zval_ptr_dtor (&zcas);
	zval_ptr_dtor (&zresult_cas);

	return retval;
}

static
protocol_binary_response_status s_append_handler (const void *cookie, const void *key, uint16_t key_len,
                                                  const void *data, uint32_t data_len, uint64_t cas, uint64_t *result_cas)
{
	return
		s_append_prepend_handler (MEMC_SERVER_ON_APPEND, cookie, key, key_len, data, data_len, cas, result_cas);
}

static
protocol_binary_response_status s_prepend_handler (const void *cookie, const void *key, uint16_t key_len,
                                                   const void *data, uint32_t data_len, uint64_t cas, uint64_t *result_cas)
{
	return
		s_append_prepend_handler (MEMC_SERVER_ON_PREPEND, cookie, key, key_len, data, data_len, cas, result_cas);
}

static
protocol_binary_response_status s_incr_decr_handler (php_memc_event_t event, const void *cookie, const void *key, uint16_t key_len, uint64_t delta,
                                                     uint64_t initial, uint32_t expiration, uint64_t *result, uint64_t *result_cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zkey, zdelta, zinital, zexpiration, zresult, zresult_cas;
	zval params[7];

	if (!MEMC_HAS_CB(event)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_STRINGL(&zkey, key, key_len);
	ZVAL_LONG(&zdelta, (long) delta);
	ZVAL_LONG(&zinital, (long) initial);
	ZVAL_LONG(&zexpiration, (long) expiration);
	ZVAL_LONG(&zresult, 0);
	ZVAL_NULL(&zresult_cas);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zdelta);
	ZVAL_COPY(&params[3], &zinital);
	ZVAL_COPY(&params[4], &zexpiration);
	ZVAL_COPY(&params[5], &zresult);
	ZVAL_COPY(&params[6], &zresult_cas);

	retval = s_invoke_php_callback (&MEMC_GET_CB(event), params, 7);

	*result = (uint64_t)zval_get_long(&zresult);

	MEMC_MAKE_RESULT_CAS(zresult_cas, *result_cas);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&params[3]);
	zval_ptr_dtor(&params[4]);
	zval_ptr_dtor(&params[5]);
	zval_ptr_dtor(&params[6]);
	zval_ptr_dtor (&zcookie);
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
	zval zcookie, zkey, zcas;
	zval params[3];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_DELETE)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_STRINGL(&zkey, key, key_len);
	ZVAL_DOUBLE(&zcas, (double) cas);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zcas);
	
	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_DELETE), params, 3);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&zcookie);
	zval_ptr_dtor(&zkey);
	zval_ptr_dtor(&zcas);
	return retval;
}

static
protocol_binary_response_status s_flush_handler(const void *cookie, uint32_t when)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zwhen;
	zval params[2];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_FLUSH)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zwhen);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_FLUSH), params, 2);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&zcookie);
	zval_ptr_dtor(&zwhen);
	return retval;
}

static
protocol_binary_response_status s_get_handler (const void *cookie, const void *key, uint16_t key_len,
                                               memcached_binary_protocol_get_response_handler response_handler)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zkey, zvalue, zflags, zresult_cas;
	zval params[5];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_GET)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zvalue);
	ZVAL_COPY(&params[3], &zflags);
	ZVAL_COPY(&params[4], &zresult_cas);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_GET), params, 5);

	/* Succeeded in getting the key */
	if (retval == PROTOCOL_BINARY_RESPONSE_SUCCESS) {
		uint32_t flags = 0;
		uint64_t result_cas = 0;

		if (Z_TYPE(zvalue) == IS_NULL) {
			zval_ptr_dtor(&params[0]);
			zval_ptr_dtor(&params[1]);
			zval_ptr_dtor(&params[2]);
			zval_ptr_dtor(&params[3]);
			zval_ptr_dtor(&params[4]);
			zval_ptr_dtor(&zcookie);
			zval_ptr_dtor(&zkey);
			zval_ptr_dtor(&zvalue);
			zval_ptr_dtor(&zflags);
			zval_ptr_dtor(&zresult_cas);
			return PROTOCOL_BINARY_RESPONSE_KEY_ENOENT;
		}

		if (Z_TYPE(zvalue) != IS_STRING) {
			convert_to_string (&zvalue);
		}

		if (Z_TYPE(zflags) == IS_LONG) {
			flags = Z_LVAL(zflags);
		}

		MEMC_MAKE_RESULT_CAS(zresult_cas, result_cas);
		retval = response_handler(cookie, key, key_len, Z_STRVAL(zvalue), Z_STRLEN(zvalue), flags, result_cas);
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&params[3]);
	zval_ptr_dtor(&params[4]);
	zval_ptr_dtor (&zcookie);
	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zvalue);
	zval_ptr_dtor (&zflags);
	zval_ptr_dtor (&zresult_cas);
	return retval;
}

static
protocol_binary_response_status s_noop_handler(const void *cookie)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie;
	zval params[1];

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_NOOP)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_COPY(&params[0], &zcookie);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_NOOP), params, 1);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor (&zcookie);
	return retval;
}

static
protocol_binary_response_status s_quit_handler(const void *cookie)
{
	zval params[1];
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie;

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_QUIT)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_COPY(&params[0], &zcookie);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_QUIT), params, 1);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor (&zcookie);
	return retval;
}



static
protocol_binary_response_status s_set_replace_handler (php_memc_event_t event, const void *cookie, const void *key, uint16_t key_len, const void *data,
                                                       uint32_t data_len, uint32_t flags, uint32_t expiration, uint64_t cas, uint64_t *result_cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zkey, zdata, zflags, zexpiration, zcas, zresult_cas;
	zval params[7];

	if (!MEMC_HAS_CB(event)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_STRINGL(&zkey, key, key_len);
	ZVAL_STRINGL(&zdata, ((char *) data), (int) data_len);
	ZVAL_LONG(&zflags, (long) flags);
	ZVAL_LONG(&zexpiration, (long) expiration);
	ZVAL_DOUBLE(&zcas, (double) cas);
	ZVAL_NULL(&zresult_cas);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zdata);
	ZVAL_COPY(&params[3], &zflags);
	ZVAL_COPY(&params[4], &zexpiration);
	ZVAL_COPY(&params[5], &zcas);
	ZVAL_COPY(&params[6], &zresult_cas);

	retval = s_invoke_php_callback (&MEMC_GET_CB(event), params, 7);

	MEMC_MAKE_RESULT_CAS(zresult_cas, *result_cas);

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&params[3]);
	zval_ptr_dtor(&params[4]);
	zval_ptr_dtor(&params[5]);
	zval_ptr_dtor(&params[6]);
	zval_ptr_dtor (&zcookie);
	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zdata);
	zval_ptr_dtor (&zflags);
	zval_ptr_dtor (&zexpiration);
	zval_ptr_dtor (&zcas);
	zval_ptr_dtor (&zresult_cas);

	return retval;
}

static
protocol_binary_response_status s_replace_handler (const void *cookie, const void *key, uint16_t key_len, const void *data,
                                                   uint32_t data_len, uint32_t flags, uint32_t expiration, uint64_t cas, uint64_t *result_cas)
{
	return
		s_set_replace_handler (MEMC_SERVER_ON_REPLACE, cookie, key, key_len, data, data_len, flags, expiration, cas, result_cas);
}

static
protocol_binary_response_status s_set_handler (const void *cookie, const void *key, uint16_t key_len, const void *data,
                                               uint32_t data_len, uint32_t flags, uint32_t expiration, uint64_t cas, uint64_t *result_cas)
{
	return
		s_set_replace_handler (MEMC_SERVER_ON_SET, cookie, key, key_len, data, data_len, flags, expiration, cas, result_cas);
}

static
protocol_binary_response_status s_stat_handler (const void *cookie, const void *key, uint16_t key_len,
                                                memcached_binary_protocol_stat_response_handler response_handler)
{
	zval params[3];
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zkey, zbody;

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_STAT)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_STRINGL(&zkey, key, key_len);
	ZVAL_NULL(&zbody);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zkey);
	ZVAL_COPY(&params[2], &zbody);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_STAT), params, 3);

	if (retval == PROTOCOL_BINARY_RESPONSE_SUCCESS) {
		if (Z_TYPE(zbody) == IS_NULL) {
			retval = response_handler(cookie, NULL, 0, NULL, 0);
		}
		else {
			if (Z_TYPE(zbody) != IS_STRING) {
				convert_to_string(&zbody);
			}
			retval = response_handler(cookie, key, key_len, Z_STRVAL(zbody), (uint32_t) Z_STRLEN(zbody));
		}
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor (&zcookie);
	zval_ptr_dtor (&zkey);
	zval_ptr_dtor (&zbody);
	return retval;
}

static
protocol_binary_response_status s_version_handler (const void *cookie,
                                                   memcached_binary_protocol_version_response_handler response_handler)
{
	zval params[2];
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND;
	zval zcookie, zversion;

	if (!MEMC_HAS_CB(MEMC_SERVER_ON_VERSION)) {
		return retval;
	}

	MEMC_MAKE_ZVAL_COOKIE(zcookie, cookie);

	ZVAL_NULL(&zversion);

	ZVAL_COPY(&params[0], &zcookie);
	ZVAL_COPY(&params[1], &zversion);

	retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_VERSION), params, 2);

	if (retval == PROTOCOL_BINARY_RESPONSE_SUCCESS) {
		if (Z_TYPE(zversion) != IS_STRING) {
			convert_to_string(&zversion);
		}

		retval = response_handler (cookie, Z_STRVAL(zversion), (uint32_t) Z_STRLEN(zversion));
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor (&zcookie);
	zval_ptr_dtor (&zversion);
	return retval;
}


// libevent callbacks

static
void s_handle_memcached_event (evutil_socket_t fd, short what, void *arg)
{
	int rc;
	short flags = 0;
	php_memc_client_t *client = (php_memc_client_t *) arg;
	memcached_protocol_event_t events;

	if (!client->on_connect_invoked) {
		if (MEMC_HAS_CB(MEMC_SERVER_ON_CONNECT)) {
			zval zremoteip, zremoteport;
			zval params[2];
			protocol_binary_response_status retval;

			struct sockaddr_in addr_in;
			socklen_t addr_in_len = sizeof(addr_in);

			if (getpeername (fd, (struct sockaddr *) &addr_in, &addr_in_len) == 0) {
				ZVAL_STRING(&zremoteip, inet_ntoa (addr_in.sin_addr));
				ZVAL_LONG(&zremoteport, ntohs (addr_in.sin_port));
			} else {
				php_error_docref(NULL, E_WARNING, "getpeername failed: %s", strerror (errno));
				ZVAL_NULL(&zremoteip);
				ZVAL_NULL(&zremoteport);
			}

			ZVAL_COPY(&params[0], &zremoteip);
			ZVAL_COPY(&params[1], &zremoteport);

			retval = s_invoke_php_callback (&MEMC_GET_CB(MEMC_SERVER_ON_CONNECT), params, 2);

			zval_ptr_dtor(&params[0]);
			zval_ptr_dtor(&params[1]);
			zval_ptr_dtor(&zremoteip);
			zval_ptr_dtor(&zremoteport);

			if (retval != PROTOCOL_BINARY_RESPONSE_SUCCESS) {
				memcached_protocol_client_destroy (client->protocol_client);
				efree (client);
				evutil_closesocket (fd);
				return;
			}
		}
		client->on_connect_invoked = 1;
	}

	events = memcached_protocol_client_work (client->protocol_client);

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
		php_error_docref (NULL, E_WARNING, "Failed to schedule events");
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
		php_error_docref (NULL, E_WARNING, "Failed to accept the client: %s", strerror (errno));
		return;
	}

	client                     = ecalloc (1, sizeof (php_memc_client_t));
	client->protocol_client    = memcached_protocol_create_client (handler->protocol_handle, sock);
	client->event_base         = handler->event_base;
	client->on_connect_invoked = 0;

	if (!client->protocol_client) {
		php_error_docref (NULL, E_WARNING, "Failed to allocate protocol client");
		efree (client);
		evutil_closesocket (sock);
		return;
	}

	// TODO: this should timeout
	rc = event_base_once (handler->event_base, sock, EV_READ, s_handle_memcached_event, client, NULL);

	if (rc != 0) {
		php_error_docref (NULL, E_WARNING, "Failed to add event for client");
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
	handler->callbacks.interface.v1.get           = s_get_handler;
	handler->callbacks.interface.v1.increment     = s_increment_handler;
	handler->callbacks.interface.v1.noop          = s_noop_handler;
	handler->callbacks.interface.v1.prepend       = s_prepend_handler;
	handler->callbacks.interface.v1.quit          = s_quit_handler;
	handler->callbacks.interface.v1.replace       = s_replace_handler;
	handler->callbacks.interface.v1.set           = s_set_handler;
	handler->callbacks.interface.v1.stat          = s_stat_handler;
	handler->callbacks.interface.v1.version       = s_version_handler;

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
	if (rc != 0) {
		php_error_docref(NULL, E_WARNING, "Failed to parse bind address");
		return -1;
	}

	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		php_error_docref(NULL, E_WARNING, "socket failed: %s", strerror (errno));
		return -1;
	}

	rc = bind (sock, (struct sockaddr *) &addr, addr_len);
	if (rc < 0) {
		php_error_docref(NULL, E_WARNING, "bind failed: %s", strerror (errno));
		return -1;
	}

	rc = listen (sock, 1024);
	if (rc < 0) {
		php_error_docref(NULL, E_WARNING, "listen failed: %s", strerror (errno));
		return -1;
	}

	rc = evutil_make_socket_nonblocking (sock);
	if (rc != 0) {
		php_error_docref(NULL, E_WARNING, "failed to make socket non-blocking: %s", strerror (errno));
		return -1;
	}

	rc = evutil_make_listen_socket_reuseable (sock);
	if (rc != 0) {
		php_error_docref(NULL, E_WARNING, "failed to make socket reuseable: %s", strerror (errno));
		return -1;
	}

	rc = evutil_make_socket_closeonexec (sock);
	if (rc != 0) {
		php_error_docref(NULL, E_WARNING, "failed to make socket closeonexec: %s", strerror (errno));
		return -1;
	}
	return sock;
}

zend_bool php_memc_proto_handler_run (php_memc_proto_handler_t *handler, zend_string *address)
{
	struct event *accept_event;
	evutil_socket_t sock = s_create_listening_socket (address->val);

	if (sock == -1) {
		return 0;
	}

	handler->event_base = event_base_new();
	if (!handler->event_base) {
		php_error_docref(NULL, E_ERROR, "failed to allocate memory: %s", strerror (errno));
	}
	accept_event = event_new (handler->event_base, sock, EV_READ | EV_PERSIST, s_accept_cb, handler);
	if (!accept_event) {
		php_error_docref(NULL, E_ERROR, "failed to allocate memory: %s", strerror (errno));
	}
	event_add (accept_event, NULL);

	switch (event_base_dispatch (handler->event_base)) {
		case -1:
			php_error_docref(NULL, E_ERROR, "event_base_dispatch() failed: %s", strerror (errno));
			return 0;
		break;

		case 1:
			php_error_docref(NULL, E_ERROR, "no events registered");
			return 0;
		break;

		default:
			return 1;
		break;
	}
}

void php_memc_proto_handler_destroy (php_memc_proto_handler_t **ptr)
{
	php_memc_proto_handler_t *handler = *ptr;

	if (handler->protocol_handle)
		memcached_protocol_destroy_instance (handler->protocol_handle);

	efree (handler);
	*ptr = NULL;
}
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: noet sw=4 ts=4 fdm=marker:
 */
