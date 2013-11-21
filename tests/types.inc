<?php
class testclass {
}

function memc_types_test ($m, $options)
{

	$data = array(
		array('boolean_true', true),
		array('boolean_false', false),
	
		array('string', "just a string"),
		array('string_empty', ""),

		array('integer_positive_integer', 10),
		array('integer_negative_integer', -10),
		array('integer_zero_integer', 0),

		array('float_positive1', 3.912131),
		array('float_positive2', 1.2131E+501),
		array('float_positive2', 1.2131E+52),
		array('float_negative', -42.123312),
		array('float_zero', 0.0),

		array('null', null),

		array('array_empty', array()),
		array('array', array(1,2,3,"foo")),

		array('object_array_empty', (object)array()),
		array('object_array', (object)array("a" => "1","b" => "2","c" => "3")),
		array('object_dummy', new testclass()),
	);

	foreach ($data as $key => $value) {
		$m->delete($key);
	}

	foreach ($data as $types) {
		$key = $types [0];
		$value = $types [1];

		$m->set($key, $value);

		$actual = $m->get($key);
		if ($value !== $actual) {
			if (is_object($actual)) {
				if ($options['ignore_object_type']) {
					$value = (object) (array) $value;
					if ($value == $actual)
						continue;
				}

				if ($value == $actual && get_class($value) == get_class($actual))
					continue;
			}
			echo "=== $types[0] ===\n";
			echo "Expected: ";
			var_dump($types[1]);
			echo "Actual: ";
			var_dump($actual);
		}
	}

	$m->flush();

	if (($actual = $m->get(uniqid ('random_key_'))) !== false) {
		echo "Expected: null";
		echo "Actual: " . gettype($actual);
	}
}

function memc_types_test_multi ($m, $options)
{
	$data = array(
		'boolean_true' => true,
		'boolean_false' => false,

		'string'       => "just a string",
		'string_empty' => "",
		'string_large' => str_repeat ('abcdef0123456789', 500),

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
		'object_array' => (object)array('a' => 1, 'b' => 2, 'c' => 3),
		'object_dummy' => new testclass(),
	);

	foreach ($data as $key => $value) {
		$m->delete($key);
	}
	$m->setMulti($data);
	$actual = $m->getMulti(array_keys($data));

	foreach ($data as $key => $value) {
		if ($value !== $actual[$key]) {
			if (is_object($value)) {
				if ($options['ignore_object_type']) {
					$value = (object) (array) $value;
					if ($value == $actual[$key])
						continue;
				}

				if ($value == $actual[$key] && get_class($value) == get_class($actual[$key]))
					continue;
			}

			echo "=== $key ===\n";
			echo "Expected: ";
			var_dump($value);
			echo "Actual: ";
			var_dump($actual[$key]);
		}
	}
}
