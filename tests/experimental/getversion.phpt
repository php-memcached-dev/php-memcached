--TEST--
Memcached::getVersion()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
var_dump($m->getVersion());

$m->addServer('localhost', 11211, 1);

$stats = $m->getVersion();
var_dump($stats);

--EXPECTF--
bool(false)
array(%d) {
  ["%s:%d"]=>
  string(%d) "%s"
}
