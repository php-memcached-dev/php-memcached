--TEST--
Memcached multi store & multi fetch type and value correctness using JSON serializer
--SKIPIF--
<?php
if (!extension_loaded("memcached")) print "skip";
if (!Memcached::HAVE_IGBINARY) print "skip json not enabled";
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
include dirname (__FILE__) . '/types.inc';

memc_run_test ('memc_types_test_multi',
	memc_create_combinations ('JSON', Memcached::SERIALIZER_JSON, true)
);

?>
--EXPECT--
TEST DONE
