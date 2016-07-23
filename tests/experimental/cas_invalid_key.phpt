--TEST--
Memcached::cas() with strange key
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

error_reporting(0);
var_dump($m->cas(0, '', true, 10));
echo $m->getResultMessage(), "\n";

var_dump($m->cas(0, ' дц jas kjjhask d ', true, 10)); # no spaces allowed
echo $m->getResultMessage(), "\n";

--EXPECTF--
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
