--TEST--
Memcached::setByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

var_dump($m->setByKey('foo', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->setByKey('', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->setByKey('foo', '', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->setByKey('foo', ' asd едц', 1, 10));
echo $m->getResultMessage(), "\n";

--EXPECT--
bool(true)
SUCCESS
bool(true)
SUCCESS
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
CLIENT ERROR
