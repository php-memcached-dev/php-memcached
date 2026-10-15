Build Status
------------
![Build Status](https://github.com/php-memcached-dev/php-memcached/actions/workflows/build-and-test.yml/badge.svg?branch=master)

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
* Supports PHP 7.0 - 8.3 or higher.
* Requires libmemcached 1.x or higher.
* Optionally supports igbinary 2.0 or higher.
* Optionally supports msgpack 2.0 or higher.
* Optionally supports phpser 0.9.0 or higher (PHP 8.2 or higher).

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

[phpser](https://github.com/iliaal/phpser) is a binary serializer for PHP
data structures that can sign what it writes. Build php-memcached with
`--enable-memcached-phpser` after installing phpser, and load phpser before
memcached in your `php.ini` (`extension=phpser` above `extension=memcached`).
Select it with `Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHPSER` or
`memcached.serializer = "phpser"`.

Without `phpser.key`, phpser behaves like igbinary: arrays, objects, and null
go through phpser, while strings, integers, floats, and booleans are stored
natively.

When `phpser.key` is set to a key of at least 32 bytes, values are HMAC-signed
for the `memcached` context and verified before decoding, so a client that can
write to the memcached servers cannot make the reader instantiate objects.
In this mode:

* Strings, booleans, null, arrays, and objects are stored as signed phpser
  values. Integers and floats stay native, so `increment()` and `decrement()`
  keep working.
* Reads accept only signed phpser values that verify, plus native integers and
  floats. Any other item, including values written by other serializers, plain
  strings, and items created by `increment()` with an initial value, reads back
  as `false` with a warning, like a value that fails to unserialize.
* `append()` and `prepend()` are refused with a warning, because they would
  break the signature.

A non-empty `phpser.key` shorter than 32 bytes also enables this mode, and
then every phpser write and read fails.
