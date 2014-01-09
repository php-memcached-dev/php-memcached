--TEST--
Memcached::getServerByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
error_reporting(0);

$m = new Memcached();
var_dump($m->getServerByKey("a"));

$m->addServer('localhost', 11211, 1);
$m->addServer('localhost', 11212, 1);
$m->addServer('localhost', 11213, 1);
$m->addServer('localhost', 11214, 1);
$m->addServer('localhost', 11215, 1);

var_dump($m->getServerByKey(""));
echo $m->getResultMessage(), "\n";
var_dump($m->getServerByKey("a"));
var_dump($m->getServerByKey("b"));
var_dump($m->getServerByKey("c"));
var_dump($m->getServerByKey("d"));
--EXPECTF--
bool(false)
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
array(%d) {
  ["host"]=>
  string(%d) "%s"
  ["port"]=>
  int(%d)
  ["weight"]=>
  int(0)
}
array(%d) {
  ["host"]=>
  string(%d) "%s"
  ["port"]=>
  int(%d)
  ["weight"]=>
  int(0)
}
array(%d) {
  ["host"]=>
  string(%d) "%s"
  ["port"]=>
  int(%d)
  ["weight"]=>
  int(0)
}
array(%d) {
  ["host"]=>
  string(%d) "%s"
  ["port"]=>
  int(%d)
  ["weight"]=>
  int(0)
}
