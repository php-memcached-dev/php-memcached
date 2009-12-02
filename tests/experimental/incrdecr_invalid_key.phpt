--TEST--
Memcached::increment() Memcached::decrement() with invalid key
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

var_dump($m->increment('', 1));
var_dump($m->decrement('', 1));
--EXPECT--
bool(false)
bool(false)
