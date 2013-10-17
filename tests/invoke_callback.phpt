--TEST--
Test that callback is invoked on new object
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

function my_func(Memcached $obj, $persistent_id = null)
{
	$obj->addServer("127.0.0.1", 11211);
}

$m = new Memcached('hi', 'my_func');

var_dump($m->getServerList());

echo "OK\n";

--EXPECTF--
array(1) {
  [0]=>
  array(2) {
    ["host"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(11211)
  }
}
OK
