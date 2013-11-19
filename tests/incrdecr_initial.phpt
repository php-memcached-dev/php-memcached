--TEST--
Memcached::increment() Memcached::decrement() with initial support
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, 1);
$m->addServer('localhost', 11211, 1);

$m->delete('foo');
var_dump($m->increment('foo', 1, 1));
var_dump($m->increment('foo', 0));
$m->delete('foo');

var_dump($m->increment('foo', 1, 1));
var_dump($m->increment('foo', 1, 1));
var_dump($m->increment('foo', 1, 1));

var_dump($m->decrement('foo', 1, 1));
var_dump($m->decrement('foo', 0));
$m->delete('foo');

$m->deleteByKey('foo', 'foo');
var_dump($m->incrementByKey('foo', 'foo', 1, 1));
var_dump($m->incrementByKey('foo', 'foo', 0));
$m->deleteByKey('foo', 'foo');

var_dump($m->incrementByKey('foo', 'foo', 1, 1));
var_dump($m->incrementByKey('foo', 'foo', 1, 1));
var_dump($m->incrementByKey('foo', 'foo', 1, 1));

var_dump($m->decrementByKey('foo', 'foo', 1, 1));
var_dump($m->decrementByKey('foo', 'foo', 0));
$m->deleteByKey('foo', 'foo');

--EXPECT--
int(1)
int(1)
int(1)
int(2)
int(3)
int(2)
int(2)
int(1)
int(1)
int(1)
int(2)
int(3)
int(2)
int(2)
