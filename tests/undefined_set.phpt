--TEST--
Set with undefined key and value
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$key = 'foobarbazDEADC0DE';
$value = array('foo' => 'bar');

$rv = $m->set($no_key, $value, 360);
var_dump($rv);


$rv = $m->set($key, $no_value, 360);
var_dump($rv);

$rv = $m->set($no_key, $no_value, 360);
var_dump($rv);

$rv = $m->set($key, $value, $no_time);
var_dump($rv);
?>
--EXPECTF--
Notice: Undefined variable: no_key in %s
bool(false)

Notice: Undefined variable: no_value in %s
bool(true)

Notice: Undefined variable: no_key in %s

Notice: Undefined variable: no_value in %s
bool(false)

Notice: Undefined variable: no_time in %s
bool(true)
