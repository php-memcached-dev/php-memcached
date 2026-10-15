--TEST--
Memcached phpser serializer with phpser.key: stored types, counters, append/prepend
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
	Memcached::OPT_COMPRESSION => false,
));

echo "-- stored types\n";
$values = array(
	'str'   => 'hello',
	'empty' => '',
	'true'  => true,
	'false' => false,
	'null'  => null,
	'arr'   => array(1, 'two' => 2),
	'obj'   => (object) array('a' => 1),
	'long'  => 42,
	'dbl'   => 1.5,
);
foreach ($values as $key => $value) {
	$m->set($key, $value);
	$back = $m->get($key);
	printf("%-5s type=%d same=%s\n", $key, phpser_raw_type($key),
		var_export(is_object($value) ? $back == $value : $back === $value, true));
}

echo "-- signed bytes match phpser_serialize_keyed()\n";
var_dump(phpser_raw_get('str')[1] === phpser_serialize_keyed('hello', 'memcached'));
var_dump(phpser_raw_get('str')[1] === phpser_forge_signed('hello', PHPSER_TEST_KEY));

echo "-- counters stay native\n";
var_dump($m->set('counter', 10));
var_dump($m->increment('counter', 5));
var_dump($m->decrement('counter', 2));
var_dump($m->get('counter'));
var_dump(phpser_raw_type('counter'));

echo "-- append/prepend are refused\n";
$m->set('ap', 'abc');
var_dump($m->append('ap', 'def'));
var_dump($m->prepend('ap', 'xyz'));
var_dump($m->appendByKey('', 'ap', 'def'));
var_dump($m->get('ap'));

echo "-- user flags are kept and do not affect the type\n";
$m->setOption(Memcached::OPT_USER_FLAGS, 0xBEEF);
$m->set('uf', array('x'));
$r = $m->get('uf', null, Memcached::GET_EXTENDED);
var_dump($r['value'] === array('x'), $r['flags'] === 0xBEEF, phpser_raw_type('uf'));

echo "-- cas writes sign too\n";
$r = $m->get('str', null, Memcached::GET_EXTENDED);
var_dump($m->cas($r['cas'], 'str', 'replaced'));
var_dump($m->get('str'), phpser_raw_type('str'));
?>
--EXPECTF--
-- stored types
str   type=8 same=true
empty type=8 same=true
true  type=8 same=true
false type=8 same=true
null  type=8 same=true
arr   type=8 same=true
obj   type=8 same=true
long  type=1 same=true
dbl   type=2 same=true
-- signed bytes match phpser_serialize_keyed()
bool(true)
bool(true)
-- counters stay native
bool(true)
int(15)
int(13)
int(13)
int(1)
-- append/prepend are refused

Warning: Memcached::append(): cannot append/prepend with the phpser serializer when phpser.key is set in %s on line %d
NULL

Warning: Memcached::prepend(): cannot append/prepend with the phpser serializer when phpser.key is set in %s on line %d
NULL

Warning: Memcached::appendByKey(): cannot append/prepend with the phpser serializer when phpser.key is set in %s on line %d
NULL
string(3) "abc"
-- user flags are kept and do not affect the type
bool(true)
bool(true)
int(8)
-- cas writes sign too
bool(true)
string(8) "replaced"
int(8)
