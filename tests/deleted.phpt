--TEST--
Memcached store & fetch type correctness
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$m->set('eisaleeoo', "foo");
$m->delete('eisaleeoo');
$v = $m->get('eisaleeoo');

if ($v !== Memcached::GET_ERROR_RETURN_VALUE) {
	echo "Wanted: ";
	var_dump(Memcached::GET_ERROR_RETURN_VALUE);
	echo "Got: ";
	var_dump($v);
}
?>
--EXPECT--
