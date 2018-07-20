--TEST--
Memcached::incrementByKey() Memcached::decrementByKey()
--SKIPIF--
<?php include "skipif.inc";?>
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

echo "Negative offset\n";
error_clear_last();
$m->incrementByKey('foo', 'foo', -1);
echo error_get_last()["message"], "\n";
var_dump($m->get('foo'));

error_clear_last();
$m->decrementByKey('foo', 'foo', -1);
echo error_get_last()["message"], "\n";
var_dump($m->get('foo'));

echo "Enormous offset\n";
$m->incrementByKey('foo', 'foo', 0x7f000000);
var_dump($m->get('foo'));

$m->decrementByKey('foo', 'foo', 0x7f000000);
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
Negative offset
Memcached::incrementByKey(): offset cannot be a negative value
int(1)
Memcached::decrementByKey(): offset cannot be a negative value
int(1)
Enormous offset
int(2130706433)
int(1)
