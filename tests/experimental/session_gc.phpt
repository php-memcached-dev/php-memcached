--TEST--
Session expiration
--SKIPIF--
<?php 
if (!extension_loaded("memcached")) print "skip"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
memcached.sess_prefix = "memc.sess.key."
session.save_path="127.0.0.1:11211"
session.save_handler = memcached
session.gc_maxlifetime = 2

--FILE--
<?php 
error_reporting(0);

session_start();
$_SESSION['foo'] = 1;
var_dump($_SESSION);
session_write_close();

$_SESSION = NULL;
var_dump($_SESSION);

session_start();
var_dump($_SESSION);
session_write_close();

sleep(3);

session_start();
var_dump($_SESSION);
session_write_close();


--EXPECT--
array(1) {
  ["foo"]=>
  int(1)
}
NULL
array(1) {
  ["foo"]=>
  int(1)
}
array(0) {
}
