--TEST--
Memcached virtual buckets
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!defined("Memcached::DISTRIBUTION_VIRTUAL_BUCKET")) die ("skip DISTRIBUTION_VIRTUAL_BUCKET not defined");
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (
							Memcached::OPT_DISTRIBUTION => Memcached::DISTRIBUTION_VIRTUAL_BUCKET
						));

var_dump ($m->setBucket (array (1, 2, 3), null, 2));

var_dump ($m->setBucket (array (1,2,2), array (1,2,2), 2));

var_dump ($m->setBucket (array ('a', 'b', 'c'), null, 2));

echo "OK\n";

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
OK
