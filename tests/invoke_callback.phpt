--TEST--
Test that callback is invoked on new object
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';

function my_func(Memcached $obj, $persistent_id = null)
{
	$obj->addServer(MEMC_SERVER_HOST, MEMC_SERVER_PORT);
}

$m = new Memcached('hi', 'my_func');
$m = new Memcached('hi', 'my_func');

var_dump($m->getServerList());

echo "OK\n";

--EXPECTF--
array(1) {
  [0]=>
  array(3) {
    ["host"]=>
    string(9) "%s"
    ["port"]=>
    int(%d)
    ["type"]=>
    string(3) "TCP"
  }
}
OK
