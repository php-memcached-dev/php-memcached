--TEST--
Memcached::get() with cache callback
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
global $runs;

$runs = 0;

function the_callback(Memcached $memc, $key, &$value, &$expiration) {
	global $runs;

	echo "Miss\n";
	var_dump($key);
	var_dump($value);

	$expiration = "10";

	$runs++;
	if ($runs == 1) {
		$value = "giving exception";
		throw new Exception("Exception in callback");
	} elseif ($runs == 2) {
		$value = "not there.";
		return 0;
	} else {
		$value = "ten";
		return 1;
	}
}

$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$m->delete('foo');
error_reporting(0);
try {
	$v = $m->get('foo', 'the_callback');
	var_dump($v);
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}
error_reporting(E_ALL);
$v = $m->get('foo', 'the_callback');
var_dump($v);
$v = $m->get('foo', 'the_callback');
var_dump($v);
$v = $m->get('foo', 'the_callback');
var_dump($v);

--EXPECT--
Miss
string(3) "foo"
NULL

Exception in callback
Miss
string(3) "foo"
NULL
NULL
Miss
string(3) "foo"
NULL
string(3) "ten"
string(3) "ten"
