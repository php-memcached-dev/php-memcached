--TEST--
Set invalid serializer
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

$default = ini_get('memcached.serializer');
$ok = ini_set('memcached.serializer', 'asdfasdffad');
if ($ok === false || ini_get('memcached.serializer') === $default) {
	echo "OK: set failed\n";
}

$ok = ini_set('memcached.serializer', null);
if ($ok === false || ini_get('memcached.serializer') === $default) {
	echo "OK: set failed\n";
}

$ok = ini_set('memcached.serializer', '');
if ($ok === false || ini_get('memcached.serializer') === $default) {
	echo "OK: set failed\n";
}

$ok = ini_set('memcached.serializer', 'php.');
if ($ok === false || ini_get('memcached.serializer') === $default) {
	echo "OK: set failed\n";
}

--EXPECTF--
OK: set failed
OK: set failed
OK: set failed
OK: set failed
