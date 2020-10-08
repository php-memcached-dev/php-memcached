--TEST--
Memcached virtual buckets
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!defined("Memcached::DISTRIBUTION_VIRTUAL_BUCKET")) die ("skip DISTRIBUTION_VIRTUAL_BUCKET not defined");
if (PHP_VERSION_ID >= 80000) die("skip PHP 7 only");
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (
							Memcached::OPT_DISTRIBUTION => Memcached::DISTRIBUTION_VIRTUAL_BUCKET
						));

var_dump ($m->setBucket (array (), null, 2));

var_dump ($m->setBucket (array (), array (), -1));

var_dump ($m->setBucket (null, array (), -1));

var_dump ($m->setBucket (array (-1), array (-1), 1));

echo "OK\n";

?>
--EXPECTF--

Warning: Memcached::setBucket(): server map cannot be empty in %s on line %d
bool(false)

Warning: Memcached::setBucket(): server map cannot be empty in %s on line %d
bool(false)

Warning: Memcached::setBucket() expects parameter 1 to be array, null given in %s on line %d
NULL

Warning: Memcached::setBucket(): the map must contain positive integers in %s on line %d
bool(false)
OK
