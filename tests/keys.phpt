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
var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

var_dump ($ascii->set ('ascii key with spaces', 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

var_dump ($ascii->set (str_repeat ('1234567890', 512), 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo "OK" . PHP_EOL;

--EXPECT--
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
OK
