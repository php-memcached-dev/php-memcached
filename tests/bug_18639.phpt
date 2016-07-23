--TEST--
Memcached::getServerByKey(): Bug pecl#18639 (Segfault in getServerByKey)
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

var_dump($m->set('test', 'test1'));
var_dump($m->getServerByKey('1'));

--EXPECTF--
bool(true)
array(3) {
  ["host"]=>
  string(9) "%s"
  ["port"]=>
  int(%d)
  ["weight"]=>
  int(%r[01]%r)
}
