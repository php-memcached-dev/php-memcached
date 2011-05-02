--TEST--
Memcached::get()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$m->delete('foo');

error_reporting(0);
var_dump($m->get('foo'));
echo $m->getResultMessage(), "\n";
var_dump($m->get(''));
echo $m->getResultMessage(), "\n";

$m->set('foo', "asdf", 10);
var_dump($m->get('foo'));
echo $m->getResultMessage(), "\n";

$m->delete('foo');
var_dump($m->get(' д foo jkh a s едц'));
echo $m->getResultMessage(), "\n";
--EXPECT--
bool(false)
NOT FOUND
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
string(4) "asdf"
SUCCESS
bool(false)
NOT FOUND
