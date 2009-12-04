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

var_dump($m->addServer('localhost', 37712, 1));

$v = $m->getMulti(array_keys($data));
var_dump(is_array($v));
var_dump(count($v) < count($data));
var_dump($m->getResultCode() == Memcached::RES_SUCCESS);

--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
