--TEST--
Check if persistent object is new or an old persistent one
--SKIPIF--
<?php include dirname(__FILE__) . "/skipif.inc";
?>
--FILE--
<?php 
$m1 = new Memcached('id1');
$m1->setOption(Memcached::OPT_PREFIX_KEY, "foo_");

var_dump($m1->isPristine());

$m1 = new Memcached('id1');
var_dump($m1->isPristine());

$m2 = new Memcached('id1');
var_dump($m2->isPristine());
// this change affects $m1
$m2->setOption(Memcached::OPT_PREFIX_KEY, "bar_");

$m3 = new Memcached('id2');
var_dump($m3->isPristine());

$m3 = new Memcached();
var_dump($m3->isPristine());
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
