--TEST--
Memcached store & fetch type and value correctness using phpser serializer
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_PHPSER) print "skip phpser not enabled";
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
include dirname (__FILE__) . '/types.inc';

memc_run_test ('memc_types_test',
	memc_create_combinations ('phpser', Memcached::SERIALIZER_PHPSER)
);

memc_run_test ('memc_types_test_multi',
	memc_create_combinations ('phpser', Memcached::SERIALIZER_PHPSER)
);

?>
--EXPECT--
TEST DONE
TEST DONE
