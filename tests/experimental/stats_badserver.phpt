--TEST--
Memcached::getStats() with bad server
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
var_dump($m->getStats());

$m->addServer('localhost', 43971, 1);
$m->addServer('localhost', 11211, 1);

$stats = $m->getStats();
var_dump($stats);

--EXPECTF--
