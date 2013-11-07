--TEST--
Test flushing buffers
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211);
$m->setOption(Memcached::OPT_NO_BLOCK, 1);
$m->setOption(Memcached::OPT_BUFFER_WRITES, 1);

$key = uniqid ('flush_key_');

var_dump ($m->set($key, 'test_val'));

$m2 = new Memcached();
$m2->addServer('127.0.0.1', 11211);

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