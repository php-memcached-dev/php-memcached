--TEST--
Memcached user flags
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
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

define ('FLAG_1', 1);
define ('FLAG_2', 2);
define ('FLAG_4', 4);
define ('FLAG_32', 32);
define ('FLAG_64', 64);
define ('FLAG_TOO_LARGE', pow(2, 16));
$x = 0;

include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (Memcached::OPT_BINARY_PROTOCOL => true));

$key = uniqid ('udf_test_');

echo "stored with flags" . PHP_EOL;

$m->set ($key, '1', 10, FLAG_1 | FLAG_4 | FLAG_64);
$udf_flags = 0;
$value = $m->get ($key, null, $x, $udf_flags);

check_flags ($udf_flags, array (FLAG_1, FLAG_4, FLAG_64));

echo "stored without flags" . PHP_EOL;
$m->set ($key, '1');
$value = $m->get ($key, null, $x, $udf_flags);

var_dump ($udf_flags == 0);
$m->set ($key, '1', 10, FLAG_TOO_LARGE);

$m->setOption(Memcached::OPT_COMPRESSION, true);
$m->setOption(Memcached::OPT_COMPRESSION_TYPE, Memcached::COMPRESSION_FASTLZ);

$m->set ($key, str_repeat ("abcdef1234567890", 200), 10, FLAG_1 | FLAG_4 | FLAG_64);

$udf_flags = 0;
$value_back = $m->get($key, null, null, $udf_flags);

check_flags ($udf_flags, array (FLAG_1, FLAG_4, FLAG_64));

echo "DONE TEST\n";
?>
--EXPECTF--
stored with flags
Flags OK
stored without flags
bool(true)

Warning: Memcached::set(): udf_flags will be limited to 65535 in %s on line %d
Flags OK
DONE TEST