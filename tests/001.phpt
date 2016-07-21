--TEST--
Check for memcached presence
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php 
echo "memcached extension is available";
?>
--EXPECT--
memcached extension is available
