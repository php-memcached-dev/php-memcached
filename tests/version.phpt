--TEST--
Get version
--SKIPIF--
<?php include "skipif.inc";?>
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
  string(%d) "%d.%d.%d"
}
OK
