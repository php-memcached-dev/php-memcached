--TEST--
Memcached multi store & fetch type and value correctness using msgpack serializer
--SKIPIF--
<?php
if (!extension_loaded("memcached")) print "skip";
if (!Memcached::HAVE_MSGPACK) print "skip msgpack not enabled";
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
include dirname (__FILE__) . '/types.inc';

memc_run_test ('memc_types_test_multi',
	memc_create_combinations ('msgpack', Memcached::SERIALIZER_MSGPACK, version_compare(phpversion("msgpack"), "0.5.5", "<="))
);

?>
--EXPECT--
TEST DONE
