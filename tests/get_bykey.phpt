--TEST--
Memcached::getByKey()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance ();

$m->set('foo', 1, 10);

var_dump($m->getByKey('foo', 'foo'));
echo $m->getResultMessage(), "\n";

var_dump($m->getByKey('', 'foo'));
echo $m->getResultMessage(), "\n";

$m->set('bar', "asdf", 10);
var_dump($m->getByKey('foo', 'bar'));
echo $m->getResultMessage(), "\n";

$m->delete('foo');
var_dump($m->getByKey(' ä foo jkh a s åäö', 'foo'));
echo $m->getResultMessage(), "\n";

--EXPECTF--
int(1)
SUCCESS
int(1)
SUCCESS
string(4) "asdf"
SUCCESS
bool(false)
NOT FOUND
