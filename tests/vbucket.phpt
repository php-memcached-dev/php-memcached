--TEST--
Memcached virtual buckets
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
var_dump ($m->setOption (Memcached::OPT_DISTRIBUTION, Memcached::DISTRIBUTION_VIRTUAL_BUCKET));

$m->addServer('127.0.0.1', 11211, 1);

var_dump ($m->setBucket (array (1, 2, 3), null, 2));

var_dump ($m->setBucket (array (1,2,2), array (1,2,2), 2));

var_dump ($m->setBucket (array ('a', 'b', 'c'), null, 2));

var_dump ($m->setBucket (array (), null, 2));

var_dump ($m->setBucket (array (), array (), -1));

var_dump ($m->setBucket (null, array (), -1));

var_dump ($m->setBucket (array (-1), array (-1), 1));

echo "OK\n";

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)

Warning: Memcached::setBucket(): server map cannot be empty in %s on line %d
bool(false)

Warning: Memcached::setBucket(): server map cannot be empty in %s on line %d
bool(false)

Warning: Memcached::setBucket() expects parameter 1 to be array, null given in %s on line %d
NULL

Warning: Memcached::setBucket(): the map must contain positive integers in %s on line %d
bool(false)
OK