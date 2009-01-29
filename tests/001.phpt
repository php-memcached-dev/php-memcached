--TEST--
Check for memcached presence
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
echo "memcached extension is available";
?>
--EXPECT--
memcached extension is available
