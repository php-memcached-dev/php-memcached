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
%s: Undefined variable%sno_key in %s
bool(false)

%s: Undefined variable%sno_value in %s
bool(true)

%s: Undefined variable%sno_key in %s

%s: Undefined variable%sno_value in %s
bool(false)

%s: Undefined variable%sno_time in %s
bool(true)
