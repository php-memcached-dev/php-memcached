--TEST--
Memcached getDelayed() and fetch() with and without cas
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

function simple_compare(array $data, array $actual, $has_cas) {
	foreach ($actual as $item) {
		if ($data[$item['key']] !== $item['value']) {
			echo "Expected:\n";
			var_dump($data[$item['key']]);
			echo "Actual:\n";
			var_dump($item);
		}

		if (isset($item['cas']) && $item['cas'] == 0) {
			echo "Invalid CAS value: ", $item['cas'], "\n";
		}
		
		if ($has_cas and !isset($item['cas'])) {
			echo "Should have cas but does not.\n";
			var_dump($item);
		}

		unset($data[$item['key']]);
	}

	if ($data) {
		echo "Missing keys:\n";
		var_dump($data);
	}
}


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

echo "fetch empty\n";
var_dump($m->getDelayed(array(), false));

echo "fetch loop\n";
$m->getDelayed(array_keys($data), false);
$tmp = array();
while ($item = $m->fetch()) $tmp[] = $item;
simple_compare($data, $tmp, false);

echo "fetch loop with cas\n";
$m->getDelayed(array_keys($data), true);
$tmp = array();
while ($item = $m->fetch()) $tmp[] = $item;
simple_compare($data, $tmp, true);

echo "fetchAll\n";
$m->getDelayed(array_keys($data), false);
$tmp = $m->fetchAll();
simple_compare($data, $tmp, false);

echo "fetchAll with cas\n";
$m->getDelayed(array_keys($data), true);
$tmp = $m->fetchAll();
simple_compare($data, $tmp, true);
--EXPECT--
fetch empty
bool(false)
fetch loop
fetch loop with cas
fetchAll
fetchAll with cas
