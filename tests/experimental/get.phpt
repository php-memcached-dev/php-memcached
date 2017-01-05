--TEST--
Memcached::get()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

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
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
