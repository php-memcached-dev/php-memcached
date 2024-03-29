--TEST--
Memcached::replaceByKey()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance ();
error_reporting(0);

$m->delete('foo');
var_dump($m->replaceByKey('kef', 'foo', 'bar', 60));
echo error_get_last()["message"], "\n";
echo $m->getResultMessage(), "\n";
var_dump($m->getByKey('kef', 'foo'));

$m->setByKey('kef', 'foo', 'kef');
var_dump($m->replaceByKey('kef', 'foo', 'bar', 60));
var_dump($m->getByKey('kef', 'foo'));

--EXPECTF--
bool(false)

%rNOT STORED|NOT FOUND%r
bool(false)
bool(true)
string(3) "bar"
