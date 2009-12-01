--TEST--
Memcached::addByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$m->delete('foo');
var_dump($m->addByKey('foo', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->addByKey('', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->addByKey('foo', '', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->addByKey('foo', ' asd едц', 1, 10));
echo $m->getResultMessage(), "\n";

--EXPECT--
bool(true)
SUCCESS
bool(false)
NOT STORED
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
CLIENT ERROR
