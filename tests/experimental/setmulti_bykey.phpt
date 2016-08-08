--TEST--
Memcached::setMultiByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php

function simple_compare(array $data, array $actual, $has_cas) {
	foreach ($actual as $key => $item) {
		if ($data[$key] !== $item) {
			echo "Expected:\n";
			var_dump($data[$key]);
			echo "Actual:\n";
			var_dump($item);
			echo "Key:\n";
			var_dump($key);
		}

		unset($data[$key]);
	}

	// keys must be strings.
	unset($data[100]);
	if ($data) {
		echo "Missing keys:\n";
		var_dump($data);
	}
}


include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

$data = array(
	'foo' => 'foo-data',
	'bar' => 'bar-data',
	'baz' => 'baz-data',
	'lol' => 'lol-data',
	'kek' => 'kek-data',
);
$data[100] = 'int-data';

var_dump($m->setMultiByKey('kef', $data, 10));
$r = $m->getMultiByKey('kef', array_keys($data));

simple_compare($data, $r, false);

--EXPECT--
bool(true)
