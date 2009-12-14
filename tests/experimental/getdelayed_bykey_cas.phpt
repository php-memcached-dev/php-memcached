--TEST--
Memcached::getDelayedByKey() with CAS
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

foreach ($data as $k => $v) {
	$m->setByKey('kef', $k, $v, 3600);
}

function myfunc() {
	$datas = func_get_args();
	if (isset($datas[1])) {
		if (isset($datas[1]['cas']) and $datas[1]['cas'] == 0) {
			echo "Invalid cas\n";
		}
		var_dump($datas[1]);
	}
}

$m->getDelayedByKey('kef', array_keys($data), true, 'myfunc');

?>
--EXPECTF--
array(3) {
  ["key"]=>
  string(3) "foo"
  ["value"]=>
  string(8) "foo-data"
  ["cas"]=>
  float(%d)
}
array(3) {
  ["key"]=>
  string(3) "bar"
  ["value"]=>
  string(8) "bar-data"
  ["cas"]=>
  float(%d)
}
array(3) {
  ["key"]=>
  string(3) "baz"
  ["value"]=>
  string(8) "baz-data"
  ["cas"]=>
  float(%d)
}
array(3) {
  ["key"]=>
  string(3) "lol"
  ["value"]=>
  string(8) "lol-data"
  ["cas"]=>
  float(%d)
}
array(3) {
  ["key"]=>
  string(3) "kek"
  ["value"]=>
  string(8) "kek-data"
  ["cas"]=>
  float(%d)
}
