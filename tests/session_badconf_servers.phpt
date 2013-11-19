--TEST--
Session bad configurations, invalid save path (server list)
--SKIPIF--
<?php 
if (!extension_loaded("memcached")) print "skip"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
memcached.sess_locking = on
memcached.sess_lock_wait = 150000
memcached.sess_prefix = "memc.sess.key."
session.save_handler = memcached

--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

error_reporting(0);
function handler($errno, $errstr) {
	echo "$errstr\n";
}

set_error_handler('handler', E_ALL);

ini_set('memcached.sess_prefix', 'memc.sess.key.');
ini_set('session.save_path', '');
session_start();
session_write_close();

--EXPECTF--
session_start(): failed to parse session.save_path
