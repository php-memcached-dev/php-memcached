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

php-memcached 3.x releases support PHP 7.0 - 7.1, and require libmemcached 1.x or higher, and optionally support igbinary 2.0 or higher.

php-memcached 2.x releases support PHP 5.2 - 5.6, and require libmemcached 0.44 or higher, and optionally support igbinary 1.0 or higher.

[libmemcached](http://libmemcached.org/libMemcached.html) version 1.0.16 or
higher is recommended for best performance and compatibility with memcached
servers.

[igbinary](https://github.com/igbinary/igbinary) is a faster and more compact
binary serializer for PHP data structures. When installing php-memcached from
source code, the igbinary module must be installed first so that php-memcached
can access its C header files. Load both modules in your `php.ini` at runtime
to begin using igbinary.
