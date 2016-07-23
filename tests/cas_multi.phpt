--TEST--
Memcached multi fetch cas & set cas
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$data = array(
	'cas_test_1' => 1,
	'cas_test_2' => 2,
);

foreach ($data as $key => $v) {
	$m->delete($key);
}

$m->setMulti($data, 10);
$actual = $m->getMulti(array_keys($data), Memcached::GET_EXTENDED);

foreach ($actual as $key => $v) {
	if (is_null($v['cas'])) {
		echo "missing cas token(s)\n";
		echo "data: ";
		var_dump($data);
		echo "actual data: ";
		var_dump($actual);
		return;
	}

	$v = $m->cas($v['cas'], $key, 11);
	if (!$v) {
		echo "Error setting key: $key value: 11 with CAS: ", $v['cas'], "\n";
		return;
	}
	$v = $m->get($key);
	if ($v !== 11) {
		echo "Wanted $key to be 11, value is: ";
		var_dump($v);
		return;
	}
}

if (array_keys($actual) !== array_keys($data)) {
	echo "missing value(s)\n";
	echo "data :";
	var_dump($data);
	echo "actual data: ";
	var_dump($actual);
	return;
}

echo "OK\n";

?>
--EXPECT--
OK