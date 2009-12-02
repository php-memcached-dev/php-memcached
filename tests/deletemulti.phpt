--TEST--
Delete multi
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);
$m->addServer('localhost', 11211, 1);

$data = array(
	'foo' => 'foo-data',
	'bar' => 'bar-data',
	'baz' => 'baz-data',
	'lol' => 'lol-data',
	'kek' => 'kek-data',
);

$m->setMulti($data, 3600);

var_dump($m->getMulti(array_keys($data)));
$m->deleteMulti(array_keys($data));
var_dump($m->getMulti(array_keys($data)));

$m->setMultiByKey("hi", $data, 3600);
var_dump($m->getMultiByKey("hi", array_keys($data)));
$m->deleteMultiByKey("hi", array_keys($data));
var_dump($m->getMultiByKey("hi", array_keys($data)));
?>
--EXPECT--
array(5) {
  ["baz"]=>
  string(8) "baz-data"
  ["lol"]=>
  string(8) "lol-data"
  ["kek"]=>
  string(8) "kek-data"
  ["foo"]=>
  string(8) "foo-data"
  ["bar"]=>
  string(8) "bar-data"
}
array(0) {
}
array(5) {
  ["foo"]=>
  string(8) "foo-data"
  ["bar"]=>
  string(8) "bar-data"
  ["baz"]=>
  string(8) "baz-data"
  ["lol"]=>
  string(8) "lol-data"
  ["kek"]=>
  string(8) "kek-data"
}
array(0) {
}