--TEST--
Memcached store & fetch type and value correctness using PHP serializer
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
include dirname (__FILE__) . '/types.inc';

memc_run_test ('memc_types_test',
	memc_create_combinations ('PHP', Memcached::SERIALIZER_PHP)
);

?>
--EXPECT--
TEST DONE
