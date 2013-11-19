--TEST--
Test for Github issue #77
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip";
      if (Memcached::LIBMEMCACHED_VERSION_HEX < 0x01000016) die ('skip too old libmemcached');
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$mc = memc_get_instance ();

$key = uniqid ("this_does_not_exist_");

$mc->touch($key, 5);
var_dump ($mc->getResultCode() == Memcached::RES_NOTFOUND);
$mc->set($key, 1, 5);

$mc->set($key, 1, 5);
var_dump ($mc->getResultCode() == Memcached::RES_SUCCESS);

echo "OK\n";

?>
--EXPECT--
bool(true)
bool(true)
OK

