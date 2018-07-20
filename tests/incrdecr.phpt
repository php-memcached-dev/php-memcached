--TEST--
Memcached::increment() Memcached::decrement()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

echo "Not there\n";
$m->delete('foo');
var_dump($m->increment('foo', 1));
var_dump($m->getResultCode());
var_dump($m->decrement('foo', 1));
var_dump($m->getResultCode());
var_dump($m->get('foo'));
var_dump($m->getResultCode());

echo "Normal\n";
$m->set('foo', 1);
var_dump($m->get('foo'));
$m->increment('foo');
var_dump($m->get('foo'));
$m->increment('foo', 2);
var_dump($m->get('foo'));
$m->decrement('foo');
var_dump($m->get('foo'));
$m->decrement('foo', 2);
var_dump($m->get('foo'));

error_reporting(0);

echo "Negative offset\n";
error_clear_last();
$m->increment('foo', -1);
echo error_get_last()["message"], "\n";
var_dump($m->get('foo'));

error_clear_last();
$m->decrement('foo', -1);
echo error_get_last()["message"], "\n";
var_dump($m->get('foo'));

echo "Enormous offset\n";
$m->increment('foo', 0x7f000000);
var_dump($m->get('foo'));

$m->decrement('foo', 0x7f000000);
var_dump($m->get('foo'));

--EXPECT--
Not there
bool(false)
int(16)
bool(false)
int(16)
bool(false)
int(16)
Normal
int(1)
int(2)
int(4)
int(3)
int(1)
Negative offset
Memcached::increment(): offset cannot be a negative value
int(1)
Memcached::decrement(): offset cannot be a negative value
int(1)
Enormous offset
int(2130706433)
int(1)
