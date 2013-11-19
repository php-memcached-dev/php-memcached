--TEST--
Memcached::getMulti()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

class Foo {
	function __toString() {
		return 'a-foo';
	}
}

$data = array();
for ($i = 0; $i < 1000; $i++) {
	$data['key' . $i] = 'value' . $i;
}
$data['a-foo'] = 'a-last';
var_dump($m->setMulti($data));

$keys = array_keys($data);
$keys['last'] = new Foo();

$v = $m->getMulti($keys);
var_dump(is_array($v));
var_dump($m->getResultCode() == Memcached::RES_SUCCESS);
if (is_array($v)) {
	foreach ($v as $key => $value) {
		if (!isset($data[$key]) or $value !== $data[$key]) {
			echo "mismatch \$data['$key'] = \n";
			var_dump($data[$key]);
			var_dump($value);
		}
	}
} else {
	echo "Result not an array\n";
}

var_dump(is_object($keys['last']));

--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
