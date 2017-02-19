--TEST--
Session persistent
--SKIPIF--
<?php 
include dirname(__FILE__) . "/skipif.inc"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
session.save_handler=memcached
memcached.sess_persistent=1
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

session_start();
$_SESSION['test']=true;
session_write_close();
session_start();
var_dump($_SESSION);
session_write_close();
?>
--EXPECT--
array(1) {
  ["test"]=>
  bool(true)
}
