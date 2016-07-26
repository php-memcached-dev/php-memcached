--TEST--
Memcached getDelayed callback
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

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

$m->getDelayed(array_keys($data), true, 'myfunc');

?>
--EXPECTF--
array(4) {
  ["key"]=>
  string(3) "foo"
  ["value"]=>
  string(8) "foo-data"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
array(4) {
  ["key"]=>
  string(3) "bar"
  ["value"]=>
  string(8) "bar-data"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
array(4) {
  ["key"]=>
  string(3) "baz"
  ["value"]=>
  string(8) "baz-data"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
array(4) {
  ["key"]=>
  string(3) "lol"
  ["value"]=>
  string(8) "lol-data"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
array(4) {
  ["key"]=>
  string(3) "kek"
  ["value"]=>
  string(8) "kek-data"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}