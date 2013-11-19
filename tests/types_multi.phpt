--TEST--
Memcached multi store & multi fetch type and value correctness
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

class testclass {
}

function types_test ($m, $options)
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

memc_run_test ('types_test', memc_get_serializer_options (true));

?>
--EXPECT--
TEST DONE
