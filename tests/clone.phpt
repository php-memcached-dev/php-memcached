--TEST--
Test cloning
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
$s = new stdClass();
$m = new Memcached();

$s = clone $s;
$m = clone $m;

echo "GOT HERE";
--EXPECTF--
Fatal error: Uncaught Error: Trying to clone an uncloneable object of class Memcached in %s:6
Stack trace:
#0 {main}
  thrown in %s on line 6
