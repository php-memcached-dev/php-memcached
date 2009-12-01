--TEST--
Memcached::getMulti() with empty array
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$v = $m->getMulti(array());
var_dump($v);

--EXPECT--
array(0) {
}
