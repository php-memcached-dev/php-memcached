--TEST--
Memcached::getByKey() with CAS
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

function the_callback(Memcached $memc, $key, &$value) {
	echo "called\n";
	$value = "1234";
	return 1;
}

$m->set('foo', 1, 10);

$cas = null;
var_dump($m->getByKey('foo', 'foo', null, $cas));
var_dump($cas);
echo $m->getResultMessage(), "\n";

$cas = null;
var_dump($m->getByKey('', 'foo', null, $cas));
var_dump($cas);
echo $m->getResultMessage(), "\n";

$m->set('bar', "asdf", 10);

$cas = null;
var_dump($m->getByKey('foo', 'bar', null, $cas));
var_dump($cas);
echo $m->getResultMessage(), "\n";

$m->delete('foo');
$cas = null;
var_dump($m->getByKey(' � foo jkh a s ���', 'foo', null, $cas));
var_dump($cas);
echo $m->getResultMessage(), "\n";

$cas = null;
var_dump($m->getByKey(' � foo jkh a s ���', '', null, $cas));
var_dump($cas);
echo $m->getResultMessage(), "\n";

$m->delete('foo');
$cas = null;
var_dump($m->getByKey('foo', 'foo', 'the_callback', $cas));
var_dump($cas);
var_dump($m->getByKey('foo', 'foo'));
--EXPECTF--
int(1)
float(%d)
SUCCESS
int(1)
float(%d)
SUCCESS
string(4) "asdf"
float(%d)
SUCCESS
bool(false)
float(0)
NOT FOUND
bool(false)
NULL
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
called
string(4) "1234"
float(0)
string(4) "1234"
