--TEST--
64-bit Memcached::increment() decrement() incrementByKey() decrementByKey()
--SKIPIF--
<?php
include "skipif.inc";
if (PHP_INT_SIZE < 8) die("skip valid for 64-bit PHP only");
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

echo "Normal\n";
$m->set('foo', 1);
var_dump($m->get('foo'));

echo "Enormous offset 64-bit\n";
$m->increment('foo', 0x100000000);
var_dump($m->get('foo'));

$m->decrement('foo', 0x100000000);
var_dump($m->get('foo'));

echo "Enormous offset 64-bit by key\n";
$m->incrementByKey('foo', 'foo', 0x100000000);
var_dump($m->get('foo'));

$m->decrementByKey('foo', 'foo', 0x100000000);
var_dump($m->get('foo'));

--EXPECT--
Normal
int(1)
Enormous offset 64-bit
int(4294967297)
int(1)
Enormous offset 64-bit by key
int(4294967297)
int(1)
