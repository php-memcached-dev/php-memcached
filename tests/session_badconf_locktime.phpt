--TEST--
Session bad configurations, lock wait time
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip";
	print "skip skip until timeout in memcached set"
?>
--INI--
memcached.sess_locking = on
memcached.sess_lock_wait = -1
memcached.sess_prefix = "memc.sess.key."
session.save_path="127.0.0.1:51312"
session.save_handler = memcached

--FILE--
<?php 
error_reporting(0);
function handler($errno, $errstr) {
	echo "$errstr\n";
}

set_error_handler('handler', E_ALL);

session_start();
session_write_close();

echo "OK\n";

--EXPECTF--
OK