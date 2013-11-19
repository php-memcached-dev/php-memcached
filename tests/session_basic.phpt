--TEST--
Session basic open, write, destroy
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

session_start();
$_SESSION['foo'] = 1;
session_write_close();

$_SESSION = NULL;

var_dump($_SESSION);
session_start();
var_dump($_SESSION);
session_write_close();

session_start();
session_destroy();

session_start();
var_dump($_SESSION);
session_write_close();


--EXPECT--
NULL
array(1) {
  ["foo"]=>
  int(1)
}
array(0) {
}
