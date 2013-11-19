--TEST--
set large data
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$key = 'foobarbazDEADC0DE';
$value = str_repeat("foo bar", 1024 * 1024);
$m->set($key, $value, 360);
var_dump($m->get($key) === $value);
?>
--EXPECT--
bool(true)
