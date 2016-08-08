--TEST--
Memcached::get/getMulti() flags
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$key1 = uniqid('memc.test.');
$key2 = uniqid('memc.test.');

$m->set ($key1, 'hello1', 20);
$m->set ($key2, 'hello2', 20);

$value = $m->get($key1);
$extended = $m->get($key1, null, Memcached::GET_EXTENDED);

var_dump ($value);
var_dump ($extended);

$values = $m->getMulti(array ($key1, $key2), Memcached::GET_PRESERVE_ORDER);
$extended = $m->getMulti(array ($key1, $key2), Memcached::GET_EXTENDED | Memcached::GET_PRESERVE_ORDER);

var_dump ($values);
var_dump ($extended);
echo "OK";

--EXPECTF--
string(6) "hello1"
array(3) {
  ["value"]=>
  string(6) "hello1"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
array(2) {
  ["memc.test.%s"]=>
  string(6) "hello1"
  ["memc.test.%s"]=>
  string(6) "hello2"
}
array(2) {
  ["memc.test.%s"]=>
  array(3) {
    ["value"]=>
    string(6) "hello1"
    ["cas"]=>
    int(%d)
    ["flags"]=>
    int(0)
  }
  ["memc.test.%s"]=>
  array(3) {
    ["value"]=>
    string(6) "hello2"
    ["cas"]=>
    int(%d)
    ["flags"]=>
    int(0)
  }
}
OK