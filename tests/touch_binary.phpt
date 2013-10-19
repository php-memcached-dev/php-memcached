--TEST--
Touch in binary mode
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip";
      if (!in_array ('touch', get_class_methods ('memcached'))) die ('skip too old libmemcached');
?>
--FILE--
<?php
$mem = new Memcached();
$mem->setOption(Memcached::OPT_BINARY_PROTOCOL,true);
$mem->addServer('127.0.0.1', 11211) or die ("Could not connect");

$key = uniqid ('touch_t_');

var_dump ($mem->get($key));
$mem->set ($key, 1);
var_dump ($mem->getResultcode());
var_dump ($mem->get($key));
$mem->touch ($key);
var_dump ($mem->getResultcode());
var_dump ($mem->get($key));
var_dump ($mem->getResultcode());

echo "OK\n";

?>
--EXPECT--
bool(false)
int(0)
int(1)
int(0)
int(1)
int(0)
OK
