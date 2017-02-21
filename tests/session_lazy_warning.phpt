--TEST--
Session lazy binary warning old libmemcached
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_SESSION) print "skip";
if (Memcached::LIBMEMCACHED_VERSION_HEX >= 0x01000018) die ('skip too recent libmemcached');
?>
--INI--
session.save_handler = memcached
memcached.sess_binary_protocol = On
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

ob_start();

session_start(['lazy_write'=>TRUE]);
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


--EXPECTF--
NULL
array(1) {
  ["foo"]=>
  int(1)
}

Warning: session_write_close(): using touch command with binary protocol is not recommended with libmemcached versions below 1.0.18, please use ascii protocol or upgrade libmemcached in %s on line %d
array(0) {
}
