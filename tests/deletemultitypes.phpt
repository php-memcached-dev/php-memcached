--TEST--
Delete multi key types
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

function dump_types($v, $k) {
	echo gettype($v) . "\n";
}

$keys = array(100, 'str');
array_walk($keys, 'dump_types');

$deleted = $m->deleteMulti($keys);
array_walk($keys, 'dump_types');

?>
--EXPECT--
integer
string
integer
string