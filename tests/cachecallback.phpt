--TEST--
Memcached::get() with cache callback
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$runs = 0;

$first_key  = uniqid ('cache_test_');
$second_key = uniqid ('cache_test_');
$third_key  = uniqid ('cache_test_');

var_dump (
$m->get ($first_key, function (Memcached $memc, $key, &$value, &$expiration) {
					$value = "hello";
					$expiration = 10;
					return true;
				})
);

var_dump ($m->get ($first_key));

var_dump (
$m->get ($second_key, function (Memcached $memc, $key, &$value, &$expiration) {
					$value = "hello";
					$expiration = 10;
					return false;
				})
);

var_dump ($m->get ($second_key));

try {
	$m->get ($third_key, function (Memcached $memc, $key, &$value, &$expiration) {
							$value = "hello";
							$expiration = 10;
							throw new Exception ('this is a test');
							return true;
						 });
} catch (Exception $e) {
	echo 'Got exception' . PHP_EOL;
}

var_dump ($m->get ($third_key));


echo "OK" . PHP_EOL;

--EXPECT--
string(5) "hello"
string(5) "hello"
bool(false)
bool(false)
Got exception
bool(false)
OK