--TEST--
Memcached multi fetch cas & set cas
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$data = array(
	'cas_test_1' => 1,
	'cas_test_2' => 2,
);

foreach ($data as $key => $v) {
	$m->delete($key);
}

$cas_tokens = array();
$m->setMulti($data, 10);
$actual = $m->getMulti(array_keys($data), $cas_tokens);

foreach ($data as $key => $v) {
	if (is_null($cas_tokens[$key])) {
		echo "missing cas token(s)\n";
		echo "data: ";
		var_dump($data);
		echo "actual data: ";
		var_dump($actual);
		echo "cas tokens: ";
		var_dump($cas_tokens);
		return;
	}

	$v = $m->cas($cas_tokens[$key], $key, 11);
	if (!$v) {
		echo "Error setting key: $key value: 11 with CAS: ", $cas_tokens[$key], "\n";
		return;
	}
	$v = $m->get($key);
	if ($v !== 11) {
		echo "Wanted $key to be 11, value is: ";
		var_dump($v);
		return;
	}
}


?>
--EXPECT--
