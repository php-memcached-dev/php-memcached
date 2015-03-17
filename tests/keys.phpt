--TEST--
Test different kind of keys
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$binary = memc_get_instance (array (
								Memcached::OPT_BINARY_PROTOCOL => true,
							));

$ascii = memc_get_instance ();
$ascii->setOption(Memcached::OPT_VERIFY_KEY, 1);

var_dump ($binary->set ('binary key with spaces', 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_SUCCESS);

var_dump ($binary->set ('binarykeywithnewline' . PHP_EOL, 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

var_dump ($ascii->set ('ascii key with spaces', 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

var_dump ($binary->set ('asciikeywithnewline' . PHP_EOL, 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

var_dump ($ascii->set (''/*empty key*/, 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

for ($i=0;$i<32;$i++) {
	var_dump ($ascii->set ('asciikeywithnonprintablechar-' . chr($i) . '-here', 'this is a test'));
	var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);
}

var_dump ($ascii->set (str_repeat ('1234567890', 512), 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo "OK" . PHP_EOL;

--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
OK
