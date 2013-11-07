--TEST--
Get version
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211);
var_dump ($m->getVersion ());

echo "OK" . PHP_EOL;
?>
--EXPECTF--
array(1) {
  ["127.0.0.1:11211"]=>
  string(6) "%d.%d.%d"
}
OK