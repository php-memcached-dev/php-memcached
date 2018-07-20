--TEST--
Memcached::fetch() with bad unserialize
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

$data = new Foo();

$m->set('foo', $data, 10);

error_reporting(0);
var_dump($m->getDelayed(array('foo'), false));
try {
	var_dump($m->fetchAll());
} catch (Exception $e) {
	echo error_get_last()["message"], "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
bool(true)
Memcached::%s(): could not unserialize value%S
123456
