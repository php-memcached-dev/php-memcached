--TEST--
Memcached::getDelayedByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

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
		} else {
			unset($datas[1]['cas']);
		}
		var_dump($datas[1]);
	}
}

$m->getDelayedByKey('kef', array_keys($data), false, 'myfunc');

?>
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
  string(3) "baz"
  ["value"]=>
  string(8) "baz-data"
}
array(2) {
  ["key"]=>
  string(3) "lol"
  ["value"]=>
  string(8) "lol-data"
}
array(2) {
  ["key"]=>
  string(3) "kek"
  ["value"]=>
  string(8) "kek-data"
}
