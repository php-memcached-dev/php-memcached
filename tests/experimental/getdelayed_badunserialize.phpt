--TEST--
Memcached::getDelayed() with bad unserialize
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

class Foo implements Serializable {
	public $serialize_throws = false;

	public function __sleep() {
		if ($this->serialize_throws) {
			throw new Exception("12");
		}
		return array();
	}

	public function __wakeup() {
		throw new Exception("1234567890");
	}

	public function serialize() {
		if ($this->serialize_throws) {
			throw new Exception("1234");
		}
		return "1234";
	}

	public function unserialize($str) {
		throw new Exception("123456");
	}
}

function mycb($memc, $key, $value) {
	return 0;
}

$data = new Foo();

$m->set('foo', $data, 10);

error_reporting(0);
try {
	var_dump($m->getDelayed(array('foo'), false, 'mycb'));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
Memcached::getDelayed(): could not unserialize value%S
123456
