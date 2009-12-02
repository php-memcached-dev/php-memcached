--TEST--
Memcached::set() with bad serialize
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

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
$m->set('foo', 10, 10);
try {
	var_dump($m->set('foo', new Foo(), 10));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}
var_dump($m->get('foo'));

--EXPECT--

1234
int(10)
