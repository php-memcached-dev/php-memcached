--TEST--
Memcached phpser serializer with phpser.key: items created by increment() with an initial value carry no type
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_PHPSER) print "skip phpser not enabled";
?>
--INI--
phpser.key=memcached-phpser-test-key-0123456789
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
include dirname(__FILE__) . '/phpser.inc';

$m = memc_get_instance(array(
	Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHPSER,
	Memcached::OPT_BINARY_PROTOCOL => true,
));

/* The server creates the item with flags 0, the string type, so get()
 * refuses it; increment() and decrement() keep working on it. */
var_dump($m->increment('c', 1, 10));
var_dump($m->increment('c', 5));
var_dump(phpser_raw_type('c'));
var_dump(@$m->get('c'));

$m->set('c2', 10);
var_dump($m->increment('c2', 1, 99));
var_dump($m->get('c2'));
?>
--EXPECT--
int(10)
int(15)
int(0)
bool(false)
int(11)
int(11)
