--TEST--
Memcached user flags
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

function check_flag ($flags, $flag)
{
    return (bool) (($flags & $flag) == $flag);
}

define ('FLAG_1', 1);
define ('FLAG_2', 2);
define ('FLAG_4', 4);
define ('FLAG_32', 32);
define ('FLAG_64', 64);
define ('FLAG_TOO_LARGE', pow(2, 16));
$x = 0;

$m = new Memcached();
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$m->addServer('127.0.0.1', 11211, 1);

$key = uniqid ('udf_test_');

echo "stored with flags" . PHP_EOL;

$m->set ($key, '1', 10, FLAG_1 | FLAG_4 | FLAG_64);
$udf_flags = 0;
$value = $m->get ($key, null, $x, $udf_flags);

var_dump (check_flag ($udf_flags, FLAG_1));
var_dump (check_flag ($udf_flags, FLAG_2));
var_dump (check_flag ($udf_flags, FLAG_4));
var_dump (check_flag ($udf_flags, FLAG_32));
var_dump (check_flag ($udf_flags, FLAG_64));

echo "stored without flags" . PHP_EOL;
$m->set ($key, '1');
$value = $m->get ($key, null, $x, $udf_flags);

var_dump ($udf_flags == 0);
$m->set ($key, '1', 10, FLAG_TOO_LARGE);

echo "DONE TEST\n";
?>
--EXPECTF--
stored with flags
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
stored without flags
bool(true)

Warning: Memcached::set(): udf_flags will be limited to 65535 in %s on line %d
DONE TEST