--TEST--
Memcached::cas() with strange key
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
$m = memc_get_instance (array (
    Memcached::OPT_BINARY_PROTOCOL => false,
    Memcached::OPT_VERIFY_KEY => true
));

error_reporting(0);
var_dump($m->cas(0, '', true, 10));
echo $m->getResultMessage(), "\n";

var_dump($m->cas(0, ' äö jas kjjhask d ', true, 10)); # no spaces allowed
echo $m->getResultMessage(), "\n";

--EXPECTF--
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
