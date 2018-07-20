--TEST--
Memcached::append()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

error_reporting(0);
$m->delete('foo');
$m->setOption(Memcached::OPT_COMPRESSION, true);
var_dump($m->append('foo', 'a'));
echo error_get_last()["message"], "\n";

$m->setOption(Memcached::OPT_COMPRESSION, false);
$m->delete('foo');
var_dump($m->append('foo', 'a'));
var_dump($m->get('foo'));
$m->set('foo', 'a');
var_dump($m->append('foo', 'b'));
var_dump($m->get('foo'));

--EXPECTF--
NULL
%s: cannot append/prepend with compression turned on
bool(false)
bool(false)
bool(true)
string(2) "ab"
