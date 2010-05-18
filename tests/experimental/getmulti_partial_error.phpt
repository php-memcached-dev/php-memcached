--TEST--
Memcached::getMulti() partial error
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

$data = array();
for ($i = 0; $i < 1000; $i++) {
	$data['key' . $i] = 'value' . $i;
}
var_dump($m->setMulti($data));

/* make sure that all keys are not there */
var_dump(count($m->deleteMulti(array("key1", "key2"))) == 2);

$v = $m->getMulti(array_keys($data));
var_dump(is_array($v));
var_dump(count($v) < count($data));
var_dump($m->getResultCode() == Memcached::RES_SUCCESS ||
	$m->getResultCode() == Memcached::RES_SOME_ERRORS);

--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
