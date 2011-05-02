--TEST--
Memcached::getStats() with bad server
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
// should be an empty array
echo $m->getResultMessage(), "\n";
var_dump($m->getStats());

$m->addServer('localhost', 43971, 1);
$m->addServer('localhost', 11211, 1);

$stats = $m->getStats();

// should be a partial result
echo $m->getResultMessage(), "\n";

// there should be stats only for one server
var_dump(count($stats));

var_dump($stats['localhost:11211']['pid'] > 0);
var_dump(count($stats['localhost:11211']));

--EXPECTF--
SUCCESS
array(0) {
}
SOME ERRORS WERE REPORTED
int(2)
bool(true)
int(%d)
