--TEST--
Check stats
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php 
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$stats = $m->getStats();
$key = MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT;

var_dump (count ($stats) === 1);
var_dump (isset ($stats[$key]));
var_dump (count ($stats[$key]) > 0);
var_dump (is_int ($stats[$key]['cmd_get']));

echo "OK";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
OK
