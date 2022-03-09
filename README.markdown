Build Status
------------
[![Build Status](https://travis-ci.org/php-memcached-dev/php-memcached.png)](https://travis-ci.org/php-memcached-dev/php-memcached)

Description
-----------
This is the [PECL memcached](https://pecl.php.net/package/memcached) extension,
using the libmemcached library to connect to memcached servers.

[memcached](https://memcached.org) is a high-performance, distributed memory
object caching system, generic in nature, but intended for use in speeding up
dynamic web applications by alleviating database load.

Building
--------

    $ phpize
    $ ./configure
    $ make
    $ make test

Dependencies
------------

php-memcached 3.x:
* Supports PHP 7.0 - 8.1.
* Requires libmemcached 1.x or higher.
* Optionally supports igbinary 2.0 or higher.
* Optionally supports msgpack 2.0 or higher.

php-memcached 2.x:
* Supports PHP 5.2 - 5.6.
* Requires libmemcached 0.44 or higher.
* Optionally supports igbinary 1.0 or higher.
* Optionally supports msgpack 0.5 or higher.

[libmemcached](http://libmemcached.org/libMemcached.html) or the new
[libmemcached-awesome](https://github.com/awesomized/libmemcached) version
1.0.18 or higher is recommended for best performance and compatibility with
memcached servers.

[igbinary](https://github.com/igbinary/igbinary) is a faster and more compact
binary serializer for PHP data structures. When installing php-memcached from
source code, the igbinary module must be installed first so that php-memcached
can access its C header files. Load both modules in your `php.ini` at runtime
to begin using igbinary.

[msgpack](https://msgpack.org) is a faster and more compact data structure
representation that is interoperable with msgpack implementations for other
languages. When installing php-memcached from source code, the msgpack module
must be installed first so that php-memcached can access its C header files.
Load both modules in your `php.ini` at runtime to begin using msgpack.
