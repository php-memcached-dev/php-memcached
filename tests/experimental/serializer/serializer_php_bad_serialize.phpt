--TEST--
Serializer: exception while serializing
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) print "skip";
	if ($_ENV['TEST_MEMC_SERIALIZER'] == 'Memcached::SERIALIZER_JSON') {
		echo "skip skip when using JSON";
	}
	if ($_ENV['TEST_MEMC_SERIALIZER'] == 'Memcached::SERIALIZER_PHP' ||
		!isset($_ENV['TEST_MEMC_SERIALIZER'])) {
		echo "skip skip when using PHP internal";
	}
--FILE--
<?php
class Foo {
	public function __sleep() {
		throw new Exception("12");
	}

	public function __wakeup() {
		throw new Exception("NULL");
	}

	public function serialize() {
		throw new Exception("1234");
	}

	public function unserialize($str) {
		throw new Exception("123456");
	}
}

$m = new Memcached();
$m->addServer('localhost', 11211, 1);
$serializer = Memcached::SERIALIZER_PHP;
if (isset($_ENV['TEST_MEMC_SERIALIZER'])) {
	eval(sprintf('$serializer = %s;', $_ENV['TEST_MEMC_SERIALIZER']));
}

var_dump($m->setOption(Memcached::OPT_SERIALIZER, $serializer));

$m->set('foo', 10, 10);
error_reporting(0);
try {
	var_dump($m->set('foo', new Foo(), 10));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}
try {
	var_dump($m->get('foo'));
} catch (Exception $e) {
	// this _should_ never happen, since the get should return 10
	// and the previous invalid set shouldn't have worked
	echo $e->getMessage(), "\n";
}

--EXPECT--
bool(true)

12
int(10)
