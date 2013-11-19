--TEST--
Memcached::add()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

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
