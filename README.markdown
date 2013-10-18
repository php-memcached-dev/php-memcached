Build Status
------------
[![Build Status](https://travis-ci.org/php-memcached-dev/php-memcached.png?branch=master)](https://travis-ci.org/php-memcached-dev/php-memcached)

Description
-----------
This extension uses libmemcached library to provide API for communicating with
memcached servers.

memcached is a high-performance, distributed memory object caching system,
generic in nature, but intended for use in speeding up dynamic web applications
by alleviating database load.

Building
--------

    $ phpize
    $ ./configure
    $ make
    $ make test

Resources
---------
 * [libmemcached](http://libmemcached.org/libMemcached.html)
 * [memcached](http://memcached.org/)
 * [igbinary](https://github.com/phadej/igbinary/)
