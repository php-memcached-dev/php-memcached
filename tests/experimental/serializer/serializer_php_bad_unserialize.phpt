--TEST--
Serializer: exception while unserializing
--SKIPIF--
<?php
include dirname(dirname(dirname(__FILE__))) . "/skipif.inc";
if ($_ENV['TEST_MEMC_SERIALIZER'] == 'Memcached::SERIALIZER_JSON') {
	echo "skip skip when using JSON";
}
--FILE--
<?php
class Foo implements Serializable {
	public function __sleep() {
		return array();
	}

	public function __wakeup() {
		throw new Exception("123456");
	}

	public function serialize() {
		return "a string describing the object";
	}

	public function unserialize($str) {
		throw new Exception("123456");
	}
}

include dirname(dirname(dirname(__FILE__))) . '/config.inc';
$m = memc_get_instance ();
$serializer = Memcached::SERIALIZER_PHP;
if (isset($_ENV['TEST_MEMC_SERIALIZER'])) {
	eval(sprintf('$serializer = %s;', $_ENV['TEST_MEMC_SERIALIZER']));
}

var_dump($m->setOption(Memcached::OPT_SERIALIZER, $serializer));

error_reporting(0);
var_dump($m->set('foo', new Foo(), 10));
try {
	var_dump($m->get('foo'));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
bool(true)
bool(true)
%scould not unserialize%s
123456
