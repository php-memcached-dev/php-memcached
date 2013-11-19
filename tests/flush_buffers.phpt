--TEST--
Test flushing buffers
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (
							Memcached::OPT_NO_BLOCK => 1,
							Memcached::OPT_BUFFER_WRITES => 1,
						));

$key = uniqid ('flush_key_');

var_dump ($m->set($key, 'test_val'));

$m2 = memc_get_instance ();

var_dump ($m2->get ($key));
var_dump ($m->flushBuffers ());
sleep (1);
var_dump ($m2->get ($key));

echo "OK" . PHP_EOL;
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
string(8) "test_val"
OK