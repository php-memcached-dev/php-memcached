--TEST--
Session basic open, write, destroy
--SKIPIF--
<?php 
include dirname(__FILE__) . "/skipif.inc"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
session.save_handler = memcached
memcached.sess_binary_protocol = Off
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

ob_start();

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
