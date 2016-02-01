--TEST--
Session bad configurations, prefix
--SKIPIF--
<?php 
if (!extension_loaded("memcached")) print "skip"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
memcached.sess_locking = on
memcached.sess_prefix = "memc.sess.key."
session.save_handler = "memcached"

--FILE--
<?php
ob_start();

include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

ini_set('memcached.sess_prefix', " \n");
session_start();
session_write_close();

echo "OK";

--EXPECTF--
Warning: ini_set(): memcached.sess_prefix cannot contain whitespace characters in %s on line %d
OK
