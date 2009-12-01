--TEST--
Memcached::getDelayedByKey() with callback exception
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$data = array(
	'foo' => 'foo-data',
	'bar' => 'bar-data',
	'baz' => 'baz-data',
	'lol' => 'lol-data',
	'kek' => 'kek-data',
);

function myfunc() {
	echo "called\n";
	throw new Exception("1234");
}

error_reporting(0);
try {
	$m->getDelayedByKey('kef', array_keys($data), false, 'myfunc');
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
called
Memcached::%s(): could not invoke%s
1234
