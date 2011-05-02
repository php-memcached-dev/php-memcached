--TEST--
Memcached::getByKey() with CAS
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

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
var_dump($m->getByKey(' д foo jkh a s едц', 'foo', null, $cas));
var_dump($cas);
echo $m->getResultMessage(), "\n";

$cas = null;
var_dump($m->getByKey(' д foo jkh a s едц', '', null, $cas));
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
NULL
NOT FOUND
bool(false)
NULL
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
called
string(4) "1234"
float(0)
string(4) "1234"
