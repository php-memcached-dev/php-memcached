--TEST--
Compress with 0 factor and get
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

ini_set('memcached.compression_factor', 0);
$array = range(1, 20000, 1);

$m->set('foo', $array, 10);
$rv = $m->get('foo');
var_dump($array === $rv);


--EXPECT--
bool(true)
