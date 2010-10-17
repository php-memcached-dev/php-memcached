--TEST--
Delete multi key types
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$m->flush();

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