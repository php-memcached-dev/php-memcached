--TEST--
Test different kind of keys
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$binary = memc_get_instance (array (
								Memcached::OPT_BINARY_PROTOCOL => true,
							));

$ascii = memc_get_instance ();

var_dump ($binary->set ('binary key with spaces', 'this is a test'));
var_dump ($binary->getResultCode ());

var_dump ($ascii->set ('ascii key with spaces', 'this is a test'));
var_dump ($ascii->getResultCode ());


echo "OK" . PHP_EOL;

--EXPECT--
bool(false)
int(33)
bool(false)
int(33)
OK
