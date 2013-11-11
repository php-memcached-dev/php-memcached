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

$x = 0;

$m = new Memcached();
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$m->addServer('127.0.0.1', 11211, 1);

$key = uniqid ('udf_test_');

echo "stored with flags" . PHP_EOL;

$m->set ($key, '1', 10, FLAG_1 | FLAG_4);
$udf_flags = 0;
$value = $m->get ($key, null, $x, $udf_flags);

var_dump (check_flag ($udf_flags, FLAG_1));
var_dump (check_flag ($udf_flags, FLAG_2));
var_dump (check_flag ($udf_flags, FLAG_4));


echo "stored without flags" . PHP_EOL;
$m->set ($key, '1');
$value = $m->get ($key, null, $x, $udf_flags);

var_dump (check_flag ($udf_flags, FLAG_1));
var_dump (check_flag ($udf_flags, FLAG_2));
var_dump (check_flag ($udf_flags, FLAG_4));


echo "DONE TEST\n";
?>
--EXPECT--
stored with flags
bool(true)
bool(false)
bool(true)
stored without flags
bool(false)
bool(false)
bool(false)
DONE TEST