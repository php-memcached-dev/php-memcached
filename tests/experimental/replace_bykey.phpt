--TEST--
Memcached::replaceByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);
error_reporting(0);

$m->delete('foo');
var_dump($m->replaceByKey('kef', 'foo', 'bar', 60));
echo $php_errormsg, "\n";
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
