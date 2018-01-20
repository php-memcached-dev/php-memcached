--TEST--
Test libmemcached encryption
--SKIPIF--
<?php
include dirname (__FILE__) . '/config.inc';
if (!extension_loaded("memcached")) die ("skip");
if (!Memcached::HAVE_ENCODING) die ("skip no set_encoding_key support enabled");
if (Memcached::LIBMEMCACHED_VERSION_HEX >= 0x01000018) die ("skip test for libmemcached lower than 1.0.18");
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();
$key = uniqid ('encoding_test_');

var_dump ($m->setEncodingKey("Hello"));
var_dump ($m->set ($key, 'set using encoding'));
var_dump ($m->get ($key));

echo "OK" . PHP_EOL;

# libmemcached < 1.0.18 goes into a bad state when the encoding key is changed,
# so php-memcached warns and returns false when trying to change the key.
var_dump ($m->setEncodingKey("World"));

echo "OK" . PHP_EOL;

?>
--EXPECTF--
bool(true)
bool(true)
string(18) "set using encoding"
OK

Warning: Memcached::setEncodingKey(): libmemcached versions less than 1.0.18 cannot change encoding key in %s on line %d
bool(false)
OK
