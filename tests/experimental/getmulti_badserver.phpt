--TEST--
Memcached::getMulti() bad server
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
var_dump($m->getMulti(array('foo', 'bar')));
echo $m->getResultMessage(), "\n";

$m->addServer('localhost', 37712, 1);

var_dump($m->getMulti(array('foo', 'bar')));
echo $m->getResultMessage(), "\n";

--EXPECT--
array(0) {
}
NO SERVERS DEFINED
array(0) {
}
SYSTEM ERROR
