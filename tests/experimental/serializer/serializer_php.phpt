--TEST--
Serializer basic
--SKIPIF--
<?php
include dirname(dirname(dirname(__FILE__))) . "/skipif.inc";
?>
--FILE--
<?php
include dirname(dirname(dirname(__FILE__))) . '/config.inc';
$m = memc_get_instance ();
$serializer = Memcached::SERIALIZER_PHP;
if (isset($_ENV['TEST_MEMC_SERIALIZER'])) {
	eval(sprintf('$serializer = %s;', $_ENV['TEST_MEMC_SERIALIZER']));
}

var_dump($m->setOption(Memcached::OPT_SERIALIZER, $serializer));

$array = range(1, 20000, 1);

$m->set('foo', $array, 10);
$rv = $m->get('foo');
var_dump($array === $rv);

--EXPECT--
bool(true)
bool(true)
