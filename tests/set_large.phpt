--TEST--
set large data
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$key = 'foobarbazDEADC0DE';
$value = str_repeat("foo bar", 1024 * 1024);
$m->set($key, $value, 360);
var_dump($m->get($key) === $value);
?>
--EXPECT--
bool(true)
