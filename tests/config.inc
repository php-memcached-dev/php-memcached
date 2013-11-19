<?php

define ("MEMC_SERVER_HOST", "127.0.0.1");
define ("MEMC_SERVER_PORT", 11211);

function memc_get_instance ()
{
	$memcached = new Memcached();
	$memcached->addServer(MEMC_SERVER_HOST, MEMC_SERVER_PORT);
	$memcached->flush ();
	return $memcached;
}

function memc_run_test ($test_function, $options = array ())
{
	foreach ($options as $option_set) {
		$memc = memc_get_instance ();

		foreach ($option_set ['options'] as $key => $value) {
			if ($memc->setOption ($key, $value) == false) {
				die ('Failed to set option: ' . $key);
			}
		}
		$test_function ($memc, $option_set);
	}
	echo "TEST DONE" . PHP_EOL;
}

function memc_create_combinations ($name, $serializer, $ignore_object_type = false)
{
	return array (
			array (
				'title' => "$name serializer, ascii protocol",
				'options' => array (
					Memcached::OPT_SERIALIZER => $serializer
				),
				'ignore_object_type' => $ignore_object_type
			),
			array (
				'title' => "$name serializer, binary protocol",
				'options' => array (
					Memcached::OPT_BINARY_PROTOCOL => true,
					Memcached::OPT_SERIALIZER => $serializer
				),
				'ignore_object_type' => $ignore_object_type
			),
	);
}

function memc_get_serializer_options () 
{
	$options = memc_create_combinations ('PHP', Memcached::SERIALIZER_PHP);

	if (Memcached::HAVE_IGBINARY) {
		$options = array_merge ($options, memc_create_combinations ('igbinary', Memcached::SERIALIZER_IGBINARY));
	}

	if (Memcached::HAVE_JSON) {
		$options = array_merge ($options, memc_create_combinations ('JSON', Memcached::SERIALIZER_JSON, true));
	}

	if (Memcached::HAVE_MSGPACK) {
		$options = array_merge ($options, memc_create_combinations ('msgpack', Memcached::SERIALIZER_MSGPACK));
	}
	return $options;
}

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

function test_bad_unserialize ($m, $options)
{
	echo "-- NEW TEST" . PHP_EOL;

	var_dump($m->set('foo', new Foo(), 10));
	error_reporting(0);

	try {
		var_dump($m->get('foo'));
	} catch (Exception $e) {
		if (version_compare(phpversion(), "5.3.0", ">=")) {
			if ($e->getPrevious()) {
				$e = $e->getPrevious();
			}
		}

		echo $php_errormsg, "\n";
		echo $e->getMessage(), "\n";
	}

	try {
		$cas = null;
		var_dump($m->get('foo', 'mycb', $cas));
	} catch (Exception $e) {
		if (version_compare(phpversion(), "5.3.0", ">=")) {
			if ($e->getPrevious()) {
				$e = $e->getPrevious();
			}
		}

		echo $php_errormsg, "\n";
		echo $e->getMessage(), "\n";
	}

	try {
		var_dump($m->get('foo', 'mycb'));
	} catch (Exception $e) {
		if (version_compare(phpversion(), "5.3.0", ">=")) {
			if ($e->getPrevious()) {
				$e = $e->getPrevious();
			}
		}

		echo $php_errormsg, "\n";
		echo $e->getMessage(), "\n";
	}

	$m->delete('foo');
	try {
		var_dump($m->get('foo', 'mycb'));
	} catch (Exception $e) {
		if (version_compare(phpversion(), "5.3.0", ">=")) {
			if ($e->getPrevious()) {
				$e = $e->getPrevious();
			}
		}

		echo $php_errormsg, "\n";
		echo $e->getMessage(), "\n";
	}
}