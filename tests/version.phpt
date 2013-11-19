--TEST--
Get version
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();
var_dump ($m->getVersion ());

echo "OK" . PHP_EOL;
?>
--EXPECTF--
array(1) {
  ["%s:%d"]=>
  string(6) "%d.%d.%d"
}
OK