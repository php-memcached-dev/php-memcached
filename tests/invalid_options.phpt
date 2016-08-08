--TEST--
Get version
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

$m = memc_get_instance ();
$m->setOption(500, 23423);
var_dump ($m->getResultCode ());

echo "OK" . PHP_EOL;
?>
--EXPECTF--
Warning: Memcached::setOption(): error setting memcached option: INVALID ARGUMENTS in %s on line %d
int(38)
OK