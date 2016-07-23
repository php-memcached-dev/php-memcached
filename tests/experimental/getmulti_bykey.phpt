--TEST--
Memcached::getMultiByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

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
