/*
 * Here we define backwards compatibility for older
 * libmemcached API:s
 *
 *      // Teddy Grenman <teddy.grenman@iki.fi>
 */

#ifndef PHP_LIBMEMCACHED_COMPAT
#define PHP_LIBMEMCACHED_COMPAT

#if !defined(LIBMEMCACHED_VERSION_HEX) || LIBMEMCACHED_VERSION_HEX < 0x00039000
/* definition from libmemcached/types.h version 0.39+ */
typedef const struct memcached_server_st *memcached_server_instance_st;
#endif

#endif
