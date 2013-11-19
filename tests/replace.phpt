--TEST--
Memcached::replace()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$m->delete('foo');
var_dump($m->replace('foo', 'bar', 60));
var_dump($m->get('foo'));

$m->set('foo', 'kef');
var_dump($m->replace('foo', 'bar', 60));
var_dump($m->get('foo'));

--EXPECT--
bool(false)
bool(false)
bool(true)
string(3) "bar"
