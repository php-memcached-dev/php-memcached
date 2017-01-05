--TEST--
Memcached constructor
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php 
$m = new Memcached();
echo get_class($m);
?>
--EXPECT--
Memcached
