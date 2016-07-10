--TEST--
Test that callback is invoked on new object
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';

function my_func(Memcached $obj, $persistent_id = null)
{
	$obj->addServer(MEMC_SERVER_HOST, MEMC_SERVER_PORT);
}

$m = new Memcached('hi', 'my_func');

$serverList = $m->getServerList();
var_dump( $serverList[0]["host"] == MEMC_SERVER_HOST ); 
var_dump( $serverList[0]["port"] == MEMC_SERVER_PORT ); 
echo "OK\n";

--EXPECTF--
bool(true)
bool(true)
OK
