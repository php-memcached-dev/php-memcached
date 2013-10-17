--TEST--
Test that correct return value is returned
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

$memcached = new Memcached();
$memcached->addServer('localhost', 5555); // Server should not exist

$result = $memcached->get('foo_not_exists');
var_dump ($result === Memcached::GET_ERROR_RETURN_VALUE);

$cas = 7;
$result = $memcached->get('foo_not_exists', null, $cas);
var_dump ($result === Memcached::GET_ERROR_RETURN_VALUE);

echo "OK\n";

?>
--EXPECT--
bool(true)
bool(true)
OK
