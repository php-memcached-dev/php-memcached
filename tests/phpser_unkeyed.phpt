--TEST--
Memcached phpser serializer without phpser.key: INI selection, native scalars, foreign types unchanged
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_PHPSER) print "skip phpser not enabled";
?>
--INI--
memcached.serializer=phpser
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
include dirname(__FILE__) . '/phpser.inc';

$m = memc_get_instance(array(Memcached::OPT_COMPRESSION => false));
var_dump($m->getOption(Memcached::OPT_SERIALIZER) === Memcached::SERIALIZER_PHPSER);

foreach (array('str' => 'hello', 'true' => true, 'long' => 7, 'dbl' => 0.5, 'arr' => array(1), 'obj' => (object) array('a' => 1)) as $key => $value) {
	$m->set($key, $value);
	echo $key, " type=", phpser_raw_type($key), "\n";
}
var_dump(phpser_raw_get('arr')[1] === phpser_serialize(array(1)));

echo "-- append works on native strings\n";
var_dump($m->append('str', ' world'), $m->get('str'));

echo "-- foreign types still decode by their stored flags\n";
phpser_raw_set('ser', MEMC_TEST_IS_SERIALIZED, serialize(array('php' => 1)));
var_dump($m->get('ser'));

echo "-- a corrupt phpser item reads as false\n";
phpser_raw_set('bad', MEMC_TEST_IS_PHPSER, 'not a phpser frame');
var_dump($m->get('bad'), $m->getResultCode());

echo "-- a signed item has a trailing tag that plain phpser rejects\n";
phpser_raw_set('signed', MEMC_TEST_IS_PHPSER, phpser_forge_signed(array('x'), PHPSER_TEST_KEY));
var_dump($m->get('signed'));

$m->setOption(Memcached::OPT_SERIALIZER, Memcached::SERIALIZER_PHP);
var_dump($m->get('arr'));
?>
--EXPECTF--
bool(true)
str type=0
true type=3
long type=1
dbl type=2
arr type=8
obj type=8
bool(true)
-- append works on native strings
bool(true)
string(11) "hello world"
-- foreign types still decode by their stored flags
array(1) {
  ["php"]=>
  int(1)
}
-- a corrupt phpser item reads as false

Warning: Memcached::get(): could not unserialize value with phpser in %s on line %d
bool(false)
int(19)
-- a signed item has a trailing tag that plain phpser rejects

Warning: Memcached::get(): could not unserialize value with phpser in %s on line %d
bool(false)
array(1) {
  [0]=>
  int(1)
}
