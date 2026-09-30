--TEST--
Memcached objects cannot be serialized
--SKIPIF--
<?php
if (!extension_loaded("memcached")) die("skip memcached is not loaded\n");
?>
--FILE--
<?php
try {
	serialize(new Memcached());
} catch (Exception $e) {
	echo $e->getMessage(), PHP_EOL;
}

try {
	// Before PHP 8.1, only the C: format reaches the handler that throws
	unserialize((PHP_VERSION_ID < 80100 ? 'C' : 'O') . ':9:"Memcached":0:{}');
} catch (Exception $e) {
	echo $e->getMessage(), PHP_EOL;
}
--EXPECT--
Serialization of 'Memcached' is not allowed
Unserialization of 'Memcached' is not allowed
