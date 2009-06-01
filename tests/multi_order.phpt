--TEST--
Memcached GET_PRESERVE_ORDER flag in getMulti
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

//$m->setMulti($data, 3600);
foreach ($data as $k => $v) {
	$m->set($k, $v, 3600);
}

$null = null;
$keys = array_keys($data);
$keys[] = 'zoo';
$got = $m->getMulti($keys, $null, Memcached::GET_PRESERVE_ORDER);

foreach ($got as $k => $v) {
	echo "$k $v\n";
}

?>
--EXPECT--
foo foo-data
bar bar-data
baz baz-data
lol lol-data
kek kek-data
zoo
