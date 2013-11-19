--TEST--
Conf settings persist.
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
include dirname (__FILE__) . '/config.inc';
$m1 = memc_get_instance (array (
							Memcached::OPT_PREFIX_KEY => 'php'
), 'id1');

$m1->set('foo', 'bar');

for ($i = 1000; $i > 0; $i--) {
	$m1 = new Memcached('id1');
	$rv = $m1->get('foo');
	if ($rv !== 'bar') {
		echo "Expected bar got:";
		var_dump($rv);
		die();
	}

	$prefix = $m1->getOption(Memcached::OPT_PREFIX_KEY);
	if ($prefix !== 'php') {
		echo "Expected prefix php got:";
		var_dump($prefix);
		die();
	}
}

echo "OK\n";

?>
--EXPECT--
OK