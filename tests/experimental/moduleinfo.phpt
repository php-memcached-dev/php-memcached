--TEST--
Memcached::phpinfo()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
ob_start();
phpinfo(INFO_MODULES);
$output = ob_get_clean();

$array = explode("\n", $output);
$array = preg_grep('/^memcached/', $array);

echo implode("\n", $array);

--EXPECTF--
memcached
memcached support => enabled
memcached.compression_factor => %f => %f
memcached.compression_threshold => %d => %d
memcached.compression_type => %s => %s
memcached.serializer => %s => %s
memcached.sess_lock_wait => %d => %d
memcached.sess_locking => %d => %d
memcached.sess_prefix => %s => %s
