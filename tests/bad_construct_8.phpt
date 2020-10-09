--TEST--
Memcached construct with bad arguments
--SKIPIF--
<?php
include "skipif.inc";
if (PHP_VERSION_ID < 80000) die("skip PHP 8 only");
?>
--FILE--
<?php 

try {
	$m = new Memcached((object)array());
} catch (TypeError $e) {
	echo $e->getMessage() . PHP_EOL;
}

class extended extends Memcached {
	public function __construct () {
	}
}

error_reporting(E_ALL);
$extended = new extended ();
var_dump ($extended->setOption (Memcached::OPT_BINARY_PROTOCOL, true));

echo "OK" . PHP_EOL;

--EXPECTF--
Memcached::__construct(): Argument #1 ($persistent_id) must be of type ?string, stdClass given

Warning: Memcached::setOption(): Memcached constructor was not called in %s
NULL
OK

