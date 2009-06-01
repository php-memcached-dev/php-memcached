--TEST--
Memcached multi store & multi fetch type and value correctness
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

class testclass {
}

$data = array(
	'boolean_true' => true,
	'boolean_false' => false,

	'string' => "just a string",
	'string_empty' => "",

	'integer_positive_integer' => 10,
	'integer_negative_integer' => -10,
	'integer_zero_integer' => 0,

	'float_positive1' => 3.912131,
	'float_positive2' => 1.2131E+52,
	'float_negative' => -42.123312,
	'float_zero' => 0.0,

	'null' => null,

	'array_empty' => array(),
	'array' => array(1,2,3,"foo"),

	'object_array_empty' => (object)array(),
	'object_array' => (object)array(1,2,3),
	'object_dummy' => new testclass(),
);

foreach ($data as $key => $value) {
	$m->delete($key);
}
$m->setMulti($data);
$actual = $m->getMulti(array_keys($data));

foreach ($data as $key => $value) {
	if ($value !== $actual[$key]) {
		if (is_object($value)
			&& $value == $actual[$key]
			&& get_class($value) == get_class($actual[$key])) {
			continue;
		}
		echo "=== $key ===\n";
		echo "Expected: ";
		var_dump($value);
		echo "Actual: ";
		var_dump($actual[$key]);
	}
}

?>
--EXPECT--
