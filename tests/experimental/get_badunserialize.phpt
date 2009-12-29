--TEST--
Memcached::get() with bad unserialize
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


function mycb(Memcached $memc, $key, &$value) {
	$value = new Foo();
	$value->serialize_throws = true;
	return 1;
}

var_dump($m->set('foo', new Foo(), 10));
error_reporting(0);

try {
	var_dump($m->get('foo'));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

try {
	$cas = null;
	var_dump($m->get('foo', 'mycb', $cas));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

try {
	var_dump($m->get('foo', 'mycb'));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

$m->delete('foo');
try {
	var_dump($m->get('foo', 'mycb'));
} catch (Exception $e) {
	echo $php_errormsg, "\n";
	echo $e->getMessage(), "\n";
}

--EXPECTF--
bool(true)
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
123456
Memcached::get(): could not unserialize value%S
1234

