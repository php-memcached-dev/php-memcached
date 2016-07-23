--TEST--
Extreme floats: max, min, Inf, -Inf, and NaN
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

$m->set('float_inf', INF);
$m->set('float_ninf', -INF);
$m->set('float_nan', NAN);
$m->set('float_big', -1.79769e308);
$m->set('float_small', -2.225e-308);
$m->set('float_subsmall', -4.9406564584125e-324);
var_dump($m->get('float_inf'));
var_dump($m->get('float_ninf'));
var_dump($m->get('float_nan'));
var_dump($m->get('float_big'));
var_dump($m->get('float_small'));
var_dump($m->get('float_subsmall'));

--EXPECT--
float(INF)
float(-INF)
float(NAN)
float(-1.79769E+308)
float(-2.225E-308)
float(-4.9406564584125E-324)
