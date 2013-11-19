--TEST--
Memcached::setMulti()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$data['foo'] = 'bar';

$data[PHP_INT_MAX] = 'int-max';
$data[-PHP_INT_MAX] = 'int-min';
$data[-PHP_INT_MAX - 1] = 'int-min';
$data[0] = 'zero';
$data[123] = 'onetwothree';
$data[-123] = 'negonetwothree';

$keys = array_map('strval', array_keys($data));

echo "Data: ";
var_dump($data);

$m->deleteMulti($keys);
echo "set keys: ";
var_dump($m->setMulti($data, 10));

echo "get: ";
$r = $m->getMulti($keys);
var_dump($r);

echo "Equal: ";
var_dump($r === $data);

--EXPECTF--
Data: array(%d) {
  ["foo"]=>
  string(3) "bar"
  [%i]=>
  string(7) "int-max"
  [%i]=>
  string(7) "int-min"
  [%i]=>
  string(7) "int-min"
  [0]=>
  string(4) "zero"
  [123]=>
  string(11) "onetwothree"
  [-123]=>
  string(14) "negonetwothree"
}
set keys: bool(true)
get: array(%d) {
  ["foo"]=>
  string(3) "bar"
  [%i]=>
  string(7) "int-max"
  [%i]=>
  string(7) "int-min"
  [%i]=>
  string(7) "int-min"
  [0]=>
  string(4) "zero"
  [123]=>
  string(11) "onetwothree"
  [-123]=>
  string(14) "negonetwothree"
}
Equal: bool(true)
