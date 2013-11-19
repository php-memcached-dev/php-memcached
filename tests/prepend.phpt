--TEST--
Memcached::prepend()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$m->delete('foo');
$m->setOption(Memcached::OPT_COMPRESSION, true);
var_dump($m->prepend('foo', 'a'));

$m->setOption(Memcached::OPT_COMPRESSION, false);
$m->delete('foo');
var_dump($m->prepend('foo', 'a'));
var_dump($m->get('foo'));
$m->set('foo', 'a');
var_dump($m->prepend('foo', 'b'));
var_dump($m->get('foo'));

--EXPECTF--
Warning: Memcached::prepend(): cannot append/prepend with compression turned on in %s on line %d
NULL
bool(false)
bool(false)
bool(true)
string(2) "ba"
