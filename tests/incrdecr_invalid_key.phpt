--TEST--
Memcached::increment() Memcached::decrement() with invalid key
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

var_dump($m->increment('', 1));
var_dump($m->decrement('', 1));
--EXPECT--
bool(false)
bool(false)
