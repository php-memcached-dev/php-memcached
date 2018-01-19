--TEST--
Test libmemcached encryption
--SKIPIF--
<?php
include dirname (__FILE__) . '/config.inc';
if (!extension_loaded("memcached")) die ("skip");
if (!Memcached::HAVE_ENCODING) die ("skip no set_encoding_key support enabled");
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

var_dump ($m->setEncodingKey("Hello"));

$key = uniqid ('encoding_test_');
var_dump ($m->set ($key, 'set using encoding'));
var_dump ($m->get ($key));

echo "OK" . PHP_EOL;

# Change the encryption key. The old value will be inaccessible.
var_dump ($m->setEncodingKey("World"));
var_dump ($m->get ($key));

echo "OK" . PHP_EOL;

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
