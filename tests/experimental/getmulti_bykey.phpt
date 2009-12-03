--TEST--
Memcached::getMultiByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$m->set('foo', 1, 10);
$m->set('bar', 2, 10);
$m->delete('baz');

$cas = array();
var_dump($m->getMultiByKey('foo', array('foo', 'bar', 'baz'), $cas, Memcached::GET_PRESERVE_ORDER));
echo $m->getResultMessage(), "\n";

$cas = array();
var_dump($m->getMultiByKey('foo', array(), $cas, Memcached::GET_PRESERVE_ORDER));
echo $m->getResultMessage(), "\n";

--EXPECT--
array(3) {
  ["foo"]=>
  int(1)
  ["bar"]=>
  int(2)
  ["baz"]=>
  NULL
}
SUCCESS
array(0) {
}
NOT FOUND
