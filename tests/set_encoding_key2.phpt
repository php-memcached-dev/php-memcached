--TEST--
Test libmemcached encryption
--SKIPIF--
<?php
include dirname (__FILE__) . '/config.inc';
if (!extension_loaded("memcached")) die ("skip");
if (!Memcached::HAVE_ENCODING) die ("skip no set_encoding_key support enabled");
if (Memcached::LIBMEMCACHED_VERSION_HEX < 0x01000018) die ("skip test for libmemcached lower than 1.0.18");
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

# Change the encryption key. The old value will be inaccessible.
var_dump ($m->setEncodingKey("World"));
var_dump ($m->get ($key));

echo "OK" . PHP_EOL;

# Restore the original key to retrieve old values again.
var_dump ($m->setEncodingKey("Hello"));
var_dump ($m->get ($key));

echo "OK" . PHP_EOL;

# Once the encoding key has changes we are no longer able to
# write new values in libmemcached < 1.0.18.
var_dump ($m->setEncodingKey("World"));
var_dump ($m->get ($key));
var_dump ($m->set ($key, 'set using encoding'));
var_dump ($m->get ($key));

echo "OK" . PHP_EOL;

?>
--EXPECT--
bool(true)
bool(true)
string(18) "set using encoding"
OK
bool(true)
bool(false)
OK
bool(true)
string(18) "set using encoding"
OK
bool(true)
bool(false)
bool(true)
string(18) "set using encoding"
OK
