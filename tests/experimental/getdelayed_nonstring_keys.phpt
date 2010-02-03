--TEST--
Memcached getDelayed non string keys
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

class Bar {
	public function __toString() {
		return 'bar';
	}
}

$data = array(
	'foo' => 'foo-data',
	'bar' => 'bar-data',
	3 => '3-data',
);

foreach ($data as $k => $v) {
	$m->set($k, $v, 3600);
}

function myfunc() {
	$datas = func_get_args();
	if (isset($datas[1])) {
		unset($datas[1]['cas']);
		var_dump($datas[1]);
	}
}

$keys = array('foo',
	$k = new Bar(),
	3,
);

$m->getDelayed($keys, false, 'myfunc');

if ($keys[0] !== 'foo') {
	echo "String 'foo' was coerced to: ";
	var_dump($keys[0]);
}

if (!$keys[1] instanceof Bar) {
	echo "Object was coerced to: ";
	var_dump($keys[1]);
}

if ($keys[2] !== 3) {
	echo "Integer 3 was coerced to: ";
	var_dump($keys[2]);
}
--EXPECT--
array(2) {
  ["key"]=>
  string(3) "foo"
  ["value"]=>
  string(8) "foo-data"
}
array(2) {
  ["key"]=>
  string(3) "bar"
  ["value"]=>
  string(8) "bar-data"
}
array(2) {
  ["key"]=>
  string(1) "3"
  ["value"]=>
  string(6) "3-data"
}
