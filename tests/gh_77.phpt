--TEST--
Test for Github issue #77
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip";
	  	if (!in_array ('touch', get_class_methods ('memcached')))
		die ('skip too old libmemcached');
?>
--FILE--
<?php
$mc = new Memcached();
$mc->addServer('127.0.0.1', 11211, 1);

$key = uniqid ("this_does_not_exist_");

$mc = new \Memcached();
$mc->setOption(\Memcached::OPT_BINARY_PROTOCOL, true);
$mc->addServer("127.0.0.1", 11211);

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

