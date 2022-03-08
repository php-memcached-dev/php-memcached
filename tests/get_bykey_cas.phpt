--TEST--
Memcached::getByKey() with CAS
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance ();

function the_callback(Memcached $memc, $key, &$value) {
	echo "called\n";
	$value = "1234";
	return true;
}

$m->set('foo', 1, 10);

$v = $m->getByKey('foo', 'foo', null, Memcached::GET_EXTENDED);
var_dump($v['value']);
var_dump($v['cas']);
echo $m->getResultMessage(), "\n";

$v = $m->getByKey('', 'foo', null, Memcached::GET_EXTENDED);
var_dump($v['value']);
var_dump($v['cas']);
echo $m->getResultMessage(), "\n";

$m->set('bar', "asdf", 10);

$v = $m->getByKey('foo', 'bar', null, Memcached::GET_EXTENDED);
var_dump($v['value']);
var_dump($v['cas']);
echo $m->getResultMessage(), "\n";

$m->delete('foo');
var_dump($m->getByKey(' ä foo jkh a s åäö', 'foo', null, Memcached::GET_EXTENDED));
echo $m->getResultMessage(), "\n";

var_dump($m->getByKey(' ä foo jkh a s åäö', '', null, Memcached::GET_EXTENDED));
echo $m->getResultMessage(), "\n";

$m->delete('foo');
var_dump($m->getByKey('foo', 'foo', 'the_callback', Memcached::GET_EXTENDED));
var_dump($m->getByKey('foo', 'foo'));
--EXPECTF--
int(1)
int(%d)
SUCCESS
int(1)
int(%d)
SUCCESS
string(4) "asdf"
int(%d)
SUCCESS
bool(false)
NOT FOUND
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
called
bool(false)
bool(false)