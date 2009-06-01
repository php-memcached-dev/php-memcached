--TEST--
Memcached store & fetch type and value correctness
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

class testclass {
}

$data = array(
	array('boolean_true', true),
	array('boolean_false', false),

	array('string', "just a string"),
	array('string_empty', ""),

	array('integer_positive_integer', 10),
	array('integer_negative_integer', -10),
	array('integer_zero_integer', 0),

	array('float_positive1', 3.912131),
	array('float_positive2', 1.2131E+52),
	array('float_negative', -42.123312),
	array('float_zero', 0.0),

	array('null', null),

	array('array_empty', array()),
	array('array', array(1,2,3,"foo")),

	array('object_array_empty', (object)array()),
	array('object_array', (object)array(1,2,3)),
	array('object_dummy', new testclass()),
);

foreach ($data as $key => $value) {
	$m->delete($key);
}

foreach ($data as $types) {
	$m->set($types[0], $types[1]);
	$actual = $m->get($types[0]);
	if ($types[1] !== $actual) {
		if (is_object($types[1])
			&& $types[1] == $actual
			&& get_class($types[1]) == get_class($actual)) {
			continue;
		}
		echo "=== $types[0] ===\n";
		echo "Expected: ";
		var_dump($types[1]);
		echo "Actual: ";
		var_dump($actual);
	}
}

?>
--EXPECT--
