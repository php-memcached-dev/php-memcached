--TEST--
Memcached getDelayed callback
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
	$m->set($k, $v, 3600);
}

function myfunc() {
	$datas = func_get_args();
	if (isset($datas[1])) {
		var_dump($datas[1]);
	}
}

$m->getDelayed(array_keys($data), false, 'myfunc');

?>
--EXPECT--
array(3) {
  ["key"]=>
  string(3) "foo"
  ["value"]=>
  string(8) "foo-data"
  ["cas"]=>
  float(0)
}
array(3) {
  ["key"]=>
  string(3) "bar"
  ["value"]=>
  string(8) "bar-data"
  ["cas"]=>
  float(0)
}
array(3) {
  ["key"]=>
  string(3) "baz"
  ["value"]=>
  string(8) "baz-data"
  ["cas"]=>
  float(0)
}
array(3) {
  ["key"]=>
  string(3) "lol"
  ["value"]=>
  string(8) "lol-data"
  ["cas"]=>
  float(0)
}
array(3) {
  ["key"]=>
  string(3) "kek"
  ["value"]=>
  string(8) "kek-data"
  ["cas"]=>
  float(0)
}