--TEST--
Memcached::append()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

error_reporting(0);
$m->delete('foo');
$m->setOption(Memcached::OPT_COMPRESSION, true);
var_dump($m->append('foo', 'a'));
echo $php_errormsg, "\n";

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
NULL
bool(true)
string(2) "ab"
