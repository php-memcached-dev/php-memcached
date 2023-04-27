--TEST--
Memcached::getMulti() with empty array
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance ();

$v = $m->getMulti(array());
var_dump($v);

--EXPECT--
array(0) {
}
