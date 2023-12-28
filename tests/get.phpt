--TEST--
Memcached::get()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance (array (
    Memcached::OPT_BINARY_PROTOCOL => false,
    Memcached::OPT_VERIFY_KEY => true
));

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
var_dump($m->get(' ä foo jkh a s åäö'));
echo $m->getResultMessage(), "\n";
--EXPECT--
bool(false)
NOT FOUND
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
string(4) "asdf"
SUCCESS
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
