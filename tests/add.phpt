--TEST--
Memcached::add()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$m->delete('foo');
var_dump($m->add('foo', 1, 60));
var_dump($m->get('foo'));
var_dump($m->add('foo', 2, 60));
var_dump($m->get('foo'));


--EXPECT--
bool(true)
int(1)
bool(false)
int(1)
