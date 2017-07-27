--TEST--
Memcached::get() with cache callback
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$runs = 0;

$first_key  = uniqid ('cache_test_');
$second_key = uniqid ('cache_test_');
$third_key  = uniqid ('cache_test_');

$m->delete($first_key);
$m->delete($second_key);
$m->delete($third_key);

var_dump (
$m->get ($first_key, function (Memcached $memc, $key, &$value, &$expiration) {
					$value = "hello";
					$expiration = 10;
					return true;
				})
);

var_dump ($m->get ($first_key));

// Get the first key again, expecting that the callback is _not_ called a second timd
$m->get ($first_key, function (Memcached $memc, $key, &$value, &$expiration) {
						throw new Exception ('This callback should not be called');
					 });

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

//
// Run through the scenarios again in GET_EXTENDED mode
//
$m->delete($first_key);
$m->delete($second_key);
$m->delete($third_key);

var_dump (
$m->get ($first_key, function (Memcached $memc, $key, &$value, &$expiration) {
					$value = [ "value" => "first_ext" ];
						// "cas"   => 12345 ];
					$expiration = 10;
					return true;
				}, Memcached::GET_EXTENDED)
);

var_dump ($m->get ($first_key, null, Memcached::GET_EXTENDED));

// Get the first key again, expecting that the callback is _not_ called a second timd
$m->get ($first_key, function (Memcached $memc, $key, &$value) {
						throw new Exception ('This callback should not be called');
					 }, Memcached::GET_EXTENDED);

var_dump (
$m->get ($second_key, function (Memcached $memc, $key, &$value, &$expiration) {
					$value = [
						"value" => "second_ext",
						"cas"   => 12345 ];
					$expiration = 10;
					return false;
				}, Memcached::GET_EXTENDED)
);

var_dump ($m->get ($second_key));
var_dump ($m->get ($second_key, null, Memcached::GET_EXTENDED));

try {
	$m->get ($third_key, function (Memcached $memc, $key, &$value, &$expiration) {
						$value = [
							"value" => "third_ext",
							"cas"   => "12345" ];
						$expiration = 10;
						throw new Exception ('this is a test');
						return true;
					 }, Memcached::GET_EXTENDED);
} catch (Exception $e) {
	echo 'Got exception' . PHP_EOL;
}

var_dump ($m->get ($third_key));

echo "OK" . PHP_EOL;

--EXPECTF--
string(5) "hello"
string(5) "hello"
bool(false)
bool(false)
Got exception
bool(false)
OK
array(3) {
  ["value"]=>
  string(9) "first_ext"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
array(3) {
  ["value"]=>
  string(9) "first_ext"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
bool(false)
bool(false)
bool(false)
Got exception
bool(false)
OK
