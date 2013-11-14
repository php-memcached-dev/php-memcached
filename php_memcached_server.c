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
#include "php_libmemcached_compat.h"
#include <libmemcachedprotocol-0.0/handler.h>

#include "php_memcached.h"
#include "php_memcached_server.h"

#include <event2/listener.h>

#undef NDEBUG
#undef _NDEBUG
#include <assert.h>


struct _php_memc_proto_handler_t {
	memcached_binary_protocol_callback_st callbacks;
	struct memcached_protocol_st *protocol_handle;
	struct event_base *event_base;
};

// memcached protocol callbacks
static
protocol_binary_response_status s_add_handler(const void *cookie, const void *key, uint16_t keylen, const void *data,
                                              uint32_t datalen, uint32_t flags, uint32_t exptime, uint64_t *cas)
{
	protocol_binary_response_status retval = PROTOCOL_BINARY_RESPONSE_SUCCESS;

	fprintf (stderr, "adding key\n");

	return retval;
}

// libevent callbacks
typedef struct _php_memc_client_t {
	struct memcached_protocol_client_st *protocol_client;
	struct event_base *event_base;
} php_memc_client_t;

static
void s_handle_memcached_event (evutil_socket_t fd, short what, void *arg)
{
	int rc;
	short flags = 0;
	php_memc_client_t *client = (php_memc_client_t *)arg;
	memcached_protocol_event_t events = memcached_protocol_client_work (client->protocol_client);

	if (events & MEMCACHED_PROTOCOL_ERROR_EVENT) {
		php_error_docref (NULL TSRMLS_CC, E_WARNING, "Client error during communication");

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

	client = ecalloc (1, sizeof (php_memc_client_t));
	client->protocol_client = memcached_protocol_create_client (handler->protocol_handle, sock);
	client->event_base = handler->event_base;

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
	/*handler->callbacks.interface.v1.append        = s_append_handler;
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

void my_run (php_memc_proto_handler_t *handler)
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