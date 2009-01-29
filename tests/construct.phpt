--TEST--
Memcached constructor
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$m = new Memcached();
echo get_class($m);
?>
--EXPECT--
Memcached
