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

$keys = array_keys($data);

$m->setMulti($data, 3600);

var_dump($m->getMulti(array_keys($data)));
var_dump($m->deleteMulti($keys));
var_dump($m->getMulti(array_keys($data)));

$m->setMultiByKey("hi", $data, 3600);
var_dump($m->getMultiByKey("hi", $keys));
var_dump($m->deleteMultiByKey("hi", $keys));
var_dump($m->getMultiByKey("hi", $keys));

$m->setMulti($data, 3600);

$keys[] = "nothere";
$keys[] = "nothere2";

$retval = $m->deleteMulti($keys);

foreach ($retval as $key => $value) {
    if ($value === Memcached::RES_NOTFOUND) {
        echo "$key NOT FOUND\n";
    }
}


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
array(5) {
  ["foo"]=>
  bool(true)
  ["bar"]=>
  bool(true)
  ["baz"]=>
  bool(true)
  ["lol"]=>
  bool(true)
  ["kek"]=>
  bool(true)
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
array(5) {
  ["foo"]=>
  bool(true)
  ["bar"]=>
  bool(true)
  ["baz"]=>
  bool(true)
  ["lol"]=>
  bool(true)
  ["kek"]=>
  bool(true)
}
array(0) {
}
nothere NOT FOUND
nothere2 NOT FOUND