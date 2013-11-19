--TEST--
Memcached store & fetch type correctness
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$m->set('eisaleeoo', "foo");
$m->delete('eisaleeoo');
$v = $m->get('eisaleeoo');

if ($v !== Memcached::GET_ERROR_RETURN_VALUE) {
	echo "Wanted: ";
	var_dump(Memcached::GET_ERROR_RETURN_VALUE);
	echo "Got: ";
	var_dump($v);
}

echo "OK\n";

?>
--EXPECT--
OK