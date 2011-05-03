--TEST--
Memcached::getServerByKey(): Bug pecl#18639 (Segfault in getServerByKey)
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
error_reporting(0);

$m = new Memcached();
$m->addServer('127.0.0.1', 11211);
var_dump($m->set('test', 'test1'));
var_dump($m->getServerByKey('1'));

--EXPECTF--
bool(true)
array(3) {
  ["host"]=>
  string(9) "127.0.0.1"
  ["port"]=>
  int(11211)
  ["weight"]=>
  int(%r[01]%r)
}
