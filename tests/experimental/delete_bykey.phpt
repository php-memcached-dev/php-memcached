--TEST--
Memcached::deleteByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$m->setByKey('keffe', 'eisaleeoo', "foo");
var_dump($m->getByKey('keffe', 'eisaleeoo'));
var_dump($m->deleteByKey('keffe', 'eisaleeoo'));
echo $m->getResultMessage(), "\n";
var_dump($m->deleteByKey('keffe', 'eisaleeoo'));
echo $m->getResultMessage(), "\n";
var_dump($m->getByKey('keffe', 'eisaleeoo'));

var_dump($m->deleteByKey('', ''));
echo $m->getResultMessage(), "\n";
var_dump($m->deleteByKey('keffe', ''));
echo $m->getResultMessage(), "\n";
var_dump($m->deleteByKey('', 'keffe'));
echo $m->getResultMessage(), "\n";
var_dump($m->deleteByKey('keffe', 'äöåasäö åaösdäf asdf'));
echo $m->getResultMessage(), "\n";
--EXPECTF--
string(3) "foo"
bool(true)
SUCCESS
bool(false)
NOT FOUND
bool(false)
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
NOT FOUND
bool(false)
%rPROTOCOL ERROR|NOT FOUND|WRITE FAILURE|CLIENT ERROR%r
