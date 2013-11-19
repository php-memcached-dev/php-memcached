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

class extended extends Memcached {
	public function __construct () {
	}
}

error_reporting(E_ALL);
$extended = new extended ();
var_dump ($extended->setOption (Memcached::OPT_BINARY_PROTOCOL, true));

echo "OK" . PHP_EOL;

--EXPECTF--
Memcached::__construct() expects parameter %s
NULL

Warning: Memcached::setOption(): Memcached constructor was not called in %s on line %d
NULL
OK