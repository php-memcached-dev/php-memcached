--TEST--
Memcached local server test
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$key = 'foobarbazDEADC0DE';
$m->set($key, array(
	'foo' => 'bar'
), 360);

var_dump($m->get($key));
?>
--EXPECT--
array(1) {
  ["foo"]=>
  string(3) "bar"
}
