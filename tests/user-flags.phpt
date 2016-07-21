--TEST--
Memcached user flags
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

function check_flags ($flags, $expected_flags)
{
    foreach ($expected_flags as $f) {
        if (($flags & $f) != $f) {
            echo "Flag {$f} is not set" . PHP_EOL;
            return;
        }
    }
    echo "Flags OK" . PHP_EOL;
}

function get_flags($m, $key) {
	return $m->get($key, null, Memcached::GET_EXTENDED)['flags'];
}

define ('FLAG_1', 1);
define ('FLAG_2', 2);
define ('FLAG_4', 4);
define ('FLAG_32', 32);
define ('FLAG_64', 64);
define ('FLAG_TOO_LARGE', pow(2, 16));

include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (Memcached::OPT_BINARY_PROTOCOL => true));

$key = uniqid ('udf_test_');

// Set with flags off
$m->set ($key, '1', 10);
$v = $m->get($key, null, Memcached::GET_EXTENDED);
var_dump($v);

// Set flags on
$m->setOption(Memcached::OPT_USER_FLAGS, FLAG_1);
$m->set ($key, '1', 10);
$m->get($key);
check_flags(get_flags($m, $key), array(FLAG_1));

// Multiple flags
$m->setOption(Memcached::OPT_USER_FLAGS, FLAG_1 | FLAG_2 | FLAG_4);
$m->set ($key, '1', 10);
$m->get($key);
check_flags(get_flags($m, $key), array(FLAG_1, FLAG_2, FLAG_4));

// Even more flags
$m->setOption(Memcached::OPT_USER_FLAGS, FLAG_1 | FLAG_2 | FLAG_4 | FLAG_32 | FLAG_64);
$m->set ($key, '1', 10);
$m->get($key);
check_flags(get_flags($m, $key), array(FLAG_1, FLAG_2, FLAG_4, FLAG_32, FLAG_64));

// User flags with get multi
$values = array(
	uniqid ('udf_test_multi_') => "first",
	uniqid ('udf_test_multi_') => "second",
	uniqid ('udf_test_multi_') => "third",
);

$m->setOption(Memcached::OPT_USER_FLAGS, FLAG_2 | FLAG_4);
$m->setMulti($values);
$m->getMulti(array_keys($values));
$flags = $m->getMulti(array_keys($values), Memcached::GET_EXTENDED);

foreach (array_keys($values) as $key) {
	check_flags($flags[$key]['flags'], array(FLAG_2, FLAG_4));
}

// User flags with compression on
$m->setOption(Memcached::OPT_USER_FLAGS, FLAG_1 | FLAG_2 | FLAG_4);
$m->setOption(Memcached::OPT_COMPRESSION, true);
$m->setOption(Memcached::OPT_COMPRESSION_TYPE, Memcached::COMPRESSION_FASTLZ);

$m->set ($key, '1', 10);
$m->get($key);
check_flags(get_flags($m, $key), array(FLAG_1, FLAG_2, FLAG_4));


// Too large flags
$m->setOption(Memcached::OPT_USER_FLAGS, FLAG_TOO_LARGE);

echo "DONE TEST\n";
?>
--EXPECTF--
array(3) {
  ["value"]=>
  string(1) "1"
  ["cas"]=>
  int(%d)
  ["flags"]=>
  int(0)
}
Flags OK
Flags OK
Flags OK
Flags OK
Flags OK
Flags OK
Flags OK

Warning: Memcached::setOption(): MEMC_OPT_USER_FLAGS must be < 65535 in %s on line %d
DONE TEST