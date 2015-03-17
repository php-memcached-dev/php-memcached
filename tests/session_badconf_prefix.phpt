--TEST--
Session bad configurations, prefix
--SKIPIF--
<?php 
    include dirname(__FILE__) . "/skipif.inc"; 
    if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
session.save_handler = memcached
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

ini_set('memcached.sess_prefix', ' sdj jkhasd ');
ini_set('memcached.sess_prefix', str_repeat('a', 512));

echo "OK";

--EXPECTF--
Warning: ini_set(): memcached.sess_prefix cannot contain whitespace or control characters in %s on line %d

Warning: ini_set(): memcached.sess_prefix too long (max: %d) in %s on line %d
OK