--TEST--
Set with undefined key and value
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$key = 'foobarbazDEADC0DE';
$value = array('foo' => 'bar');

// silent to hide:
// Warning: Undefined variable
// Deprecated: Memcached::set(): Passing null to parameter (PHP 8.1)

$rv = @$m->set($no_key, $value, 360);
var_dump($rv);


$rv = @$m->set($key, $no_value, 360);
var_dump($rv);

$rv = @$m->set($no_key, $no_value, 360);
var_dump($rv);

$rv = @$m->set($key, $value, $no_time);
var_dump($rv);
?>
--EXPECTF--
bool(false)
bool(true)
bool(false)
bool(true)
