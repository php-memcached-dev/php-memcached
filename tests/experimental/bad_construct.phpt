--TEST--
Memcached construct with bad arguments
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
error_reporting(0);
$m = new Memcached((object)array());
echo $php_errormsg, "\n";
var_dump($m);

--EXPECTF--
Memcached::__construct() expects parameter %s
NULL
