--TEST--
Memcached::fetch() with bad unserialize
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

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
	var_dump($m->fetch());
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
bool(true)
Memcached::%s(): could not unserialize value%S
123456
