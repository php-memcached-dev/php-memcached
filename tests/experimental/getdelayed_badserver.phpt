--TEST--
Memcached::getDelayedByKey() with bad server
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 48813, 1);

$data = array(
	'foo' => 'foo-data',
	'bar' => 'bar-data',
	'baz' => 'baz-data',
	'lol' => 'lol-data',
	'kek' => 'kek-data',
);

function myfunc() {
	$datas = func_get_args();
	var_dump($datas);
}

$m->getDelayedByKey('kef', array_keys($data), false, 'myfunc');

--EXPECT--
