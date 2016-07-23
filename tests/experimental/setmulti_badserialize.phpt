--TEST--
Memcached::setMultiByKey() with bad serialize
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

class Foo implements Serializable {
	public function __sleep() {
		throw new Exception("12");
	}

	public function __wakeup() {
		throw new Exception("1234567890");
	}

	public function serialize() {
		throw new Exception("1234");
	}

	public function unserialize($str) {
		throw new Exception("123456");
	}
}

error_reporting(0);
$m->setByKey('kef', 'foo', 10, 10);
try {
	var_dump($m->setMultiByKey('kef', array('foo' => new Foo()), 10));
} catch (Exception $e) {
	if (version_compare(phpversion(), "5.3.0", ">=")) {
		if ($e->getPrevious()) {
			$e = $e->getPrevious();
		}
	}

	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}
var_dump($m->getByKey('kef', 'foo'));

--EXPECT--
Memcached::setMultiByKey(): failed to set key foo
1234
int(10)
