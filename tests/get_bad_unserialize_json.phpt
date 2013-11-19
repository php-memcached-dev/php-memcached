--TEST--
Memcached::get() with bad unserialize with JSON serializer
--SKIPIF--
<?php
if (!extension_loaded("memcached")) print "skip";
if (!Memcached::HAVE_JSON) print "skip no JSON support";
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

memc_run_test ('test_bad_unserialize', memc_create_combinations ('PHP', Memcached::SERIALIZER_JSON, false));

--EXPECTF--
-- NEW TEST
bool(true)
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
1234
-- NEW TEST
bool(true)
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
1234
TEST DONE
