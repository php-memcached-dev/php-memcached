--TEST--
Memcached::incrementByKey() Memcached::decrementByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

echo "Not there\n";
$m->delete('foo');
var_dump($m->incrementByKey('foo', 'foo', 1));
var_dump($m->decrementByKey('foo', 'foo', 1));
var_dump($m->get('foo'));

echo "Normal\n";
$m->set('foo', 1);
var_dump($m->get('foo'));
$m->incrementByKey('foo', 'foo');
var_dump($m->get('foo'));
$m->incrementByKey('foo', 'foo', 2);
var_dump($m->get('foo'));
$m->decrementByKey('foo', 'foo');
var_dump($m->get('foo'));
$m->decrementByKey('foo', 'foo', 2);
var_dump($m->get('foo'));

error_reporting(0);
echo "Invalid offset\n";
$m->incrementByKey('foo', 'foo', -1);
echo $php_errormsg, "\n";
var_dump($m->get('foo'));
$m->decrementByKey('foo', 'foo', -1);
echo $php_errormsg, "\n";
var_dump($m->get('foo'));

--EXPECT--
Not there
bool(false)
bool(false)
bool(false)
Normal
int(1)
int(2)
int(4)
int(3)
int(1)
Invalid offset
Memcached::incrementByKey(): offset has to be > 0
int(1)
Memcached::decrementByKey(): offset has to be > 0
int(1)
