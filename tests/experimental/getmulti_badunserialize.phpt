--TEST--
Memcached::getMulti() with bad unserialize
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

class Foo implements Serializable {
	public function __sleep() {
		return array();
	}

	public function __wakeup() {
		throw new Exception("1234567890");
	}

	public function serialize() {
		return "1234";
	}

	public function unserialize($str) {
		throw new Exception("123456");
	}
}

var_dump($m->set('bar', "12", 10));
var_dump($m->set('foo', new Foo(), 10));
error_reporting(0);

try {
	var_dump($m->getMulti(array('bar', 'foo')));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
bool(true)
bool(true)
Memcached::%s(): could not unserialize%s
123456
