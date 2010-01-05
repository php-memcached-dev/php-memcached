--TEST--
Delete multi with integer keys
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);
$m->addServer('localhost', 11211, 1);

$data = array(
	1 => '1-data',
	2 => '2-data',
	'foo' => 'foo-data',
	3 => '3-data',
	4 => '4-data',
	'kek' => 'kek-data',
);

$str_keys = array_map('strval', array_keys($data));

echo "set: ";
$f = true;
foreach ($data as $key => $value) {
	$f = $m->set(strval($key), $value, 2) && $f;
}
echo var_dump($f);

$data[5] = 'not-there';

echo "delete: ";
var_dump($m->deleteMulti(array_keys($data)));
echo "Should be empty / not found: ";
var_dump($m->getMulti($str_keys));

--EXPECTF--
set: bool(true)
delete: array(7) {
  [1]=>
  bool(true)
  [2]=>
  bool(true)
  ["foo"]=>
  bool(true)
  [3]=>
  bool(true)
  [4]=>
  bool(true)
  ["kek"]=>
  bool(true)
  [5]=>
  int(%d)
}
Should be empty / not found: array(0) {
}
