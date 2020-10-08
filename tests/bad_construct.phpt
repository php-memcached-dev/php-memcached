--TEST--
Memcached construct with bad arguments
--SKIPIF--
<?php
include "skipif.inc";
if (PHP_VERSION_ID >= 80000) die("skip PHP 7 only");
?>
--FILE--
<?php 

$m = new Memcached((object)array());
echo error_get_last()["message"], "\n";
var_dump($m);

class extended extends Memcached {
	public function __construct () {
	}
}

error_reporting(E_ALL);
$extended = new extended ();
var_dump ($extended->setOption (Memcached::OPT_BINARY_PROTOCOL, true));

echo "OK" . PHP_EOL;

--EXPECTF--
Warning: Memcached::__construct() expects parameter 1 to be string, object given in %s on line 3
Memcached::__construct() expects parameter 1 to be string, object given
object(Memcached)#1 (0) {
}

Warning: Memcached::setOption(): Memcached constructor was not called in %s on line 14
NULL
OK

