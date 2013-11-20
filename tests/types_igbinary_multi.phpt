--TEST--
Memcached multi store & multi fetch type and value correctness using igbinary serializer
--SKIPIF--
<?php
if (!extension_loaded("memcached")) print "skip";
if (!Memcached::HAVE_IGBINARY) print "skip igbinary not enabled";
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
include dirname (__FILE__) . '/types.inc';

memc_run_test ('memc_types_test_multi',
	memc_create_combinations ('igbinary', Memcached::SERIALIZER_IGBINARY)
);

?>
--EXPECT--
TEST DONE
