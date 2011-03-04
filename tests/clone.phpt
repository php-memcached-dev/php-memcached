--TEST--
Test cloning
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$s = new stdClass();
$m = new Memcached();

$s = clone $s;
$m = clone $m;

echo "GOT HERE";
--EXPECTF--
Fatal error: Trying to clone an uncloneable object of class Memcached in %s on line %d