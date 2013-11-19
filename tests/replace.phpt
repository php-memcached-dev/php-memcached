--TEST--
Memcached::replace()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

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
