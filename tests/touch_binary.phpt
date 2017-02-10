--TEST--
Touch in binary mode
--SKIPIF--
<?php
$min_version = "1.4.8"; //TOUCH is added since 1.4.8
include dirname(__FILE__) . "/skipif.inc";
// The touch command in binary mode will work in libmemcached 1.0.16, but runs out the timeout clock
// See https://github.com/php-memcached-dev/php-memcached/issues/310 for further explanation
// The problem is fixed fully in libmemcached 1.0.18, so we'll focus tests on that version
if (Memcached::LIBMEMCACHED_VERSION_HEX < 0x01000018) die ('skip too old libmemcached');
?>
--FILE--
<?php

function resolve_to_constant ($code)
{
	$refl = new ReflectionClass ('memcached');
	$c = $refl->getConstants ();
	
	foreach ($c as $name => $value) {
		if (strpos ($name, 'RES_') === 0 && $value == $code)
			return $name;
	}
}

function status_print ($op, $mem, $expected)
{
	$code = $mem->getResultcode();

	if ($code == $expected)
		echo "${op} status code as expected" . PHP_EOL;
	else {
		$expected = resolve_to_constant ($expected);
		$code = resolve_to_constant ($code);
		
		echo "${op} status code mismatch, expected ${expected} but got ${code}" . PHP_EOL;
	}
}

include dirname (__FILE__) . '/config.inc';
$mem = memc_get_instance (array (Memcached::OPT_BINARY_PROTOCOL => true));

$key = uniqid ('touch_t_');

$mem->get($key);
status_print ('get', $mem, Memcached::RES_NOTFOUND);

$mem->set ($key, 1);
status_print ('set', $mem, Memcached::RES_SUCCESS);

$mem->get($key);
status_print ('get', $mem, Memcached::RES_SUCCESS);

$mem->touch ($key, 10);
status_print ('touch', $mem, Memcached::RES_SUCCESS);

$mem->get($key);
status_print ('get', $mem, Memcached::RES_SUCCESS);

$mem->get($key);
status_print ('get', $mem, Memcached::RES_SUCCESS);

echo "OK\n";

?>
--EXPECT--
get status code as expected
set status code as expected
get status code as expected
touch status code as expected
get status code as expected
get status code as expected
OK
