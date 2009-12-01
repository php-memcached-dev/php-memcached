--TEST--
Memcached::replace()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);
error_reporting(0);

$m->delete('foo');
var_dump($m->replace('foo', 'bar', 60));
echo $php_errormsg, "\n";
var_dump($m->get('foo'));

$m->set('foo', 'kef');
var_dump($m->replace('foo', 'bar', 60));
var_dump($m->get('foo'));

--EXPECTF--
bool(false)

NULL
bool(true)
string(3) "bar"
