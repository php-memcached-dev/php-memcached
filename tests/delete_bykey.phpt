--TEST--
Memcached::deleteByKey()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance (array (
    Memcached::OPT_BINARY_PROTOCOL => false,
    Memcached::OPT_VERIFY_KEY => true
));

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
var_dump($m->deleteByKey('keffe', 'äöåasäö åaösdäf asdf')); # no spaces allowed
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
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
