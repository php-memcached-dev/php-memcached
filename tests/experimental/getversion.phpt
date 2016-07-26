--TEST--
Memcached::getVersion()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
$m = new Memcached();
var_dump($m->getVersion());

include dirname(dirname(__FILE__)) . "/config.inc";
$m = memc_get_instance ();

$stats = $m->getVersion();
var_dump($stats);

--EXPECTF--
bool(false)
array(%d) {
  ["%s:%d"]=>
  string(%d) "%s"
}
