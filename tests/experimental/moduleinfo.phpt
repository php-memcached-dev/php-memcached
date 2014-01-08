--TEST--
Memcached::phpinfo()
--SKIPIF--
<?php 
if (!extension_loaded("memcached")) print "skip"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
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
memcached.sess_binary => %d => %d
memcached.sess_connect_timeout => %d => %d
memcached.sess_consistent_hash => %d => %d
memcached.sess_lock_expire => %d => %d
memcached.sess_lock_max_wait => %d => %d
memcached.sess_lock_wait => %d => %d
memcached.sess_locking => %d => %d
memcached.sess_number_of_replicas => %d => %d
memcached.sess_prefix => %s => %s
memcached.sess_randomize_replica_read => %d => %d
memcached.sess_remove_failed => %d => %d
%rmemcached.use_sasl => %d => %d
|%rmemcached.store_retry_count => %d => %d