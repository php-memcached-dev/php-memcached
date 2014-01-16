--TEST--
Serializer basic
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) print "skip";
?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);
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
