--TEST--
Test valid and invalid keys - binary
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$binary = memc_get_instance (array (
		Memcached::OPT_BINARY_PROTOCOL => true,
	));

echo 'BINARY: SPACES' . PHP_EOL;
var_dump ($binary->set ('binary key with spaces', 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_SUCCESS);

echo 'BINARY: NEWLINE' . PHP_EOL;
var_dump ($binary->set ('binarykeywithnewline' . PHP_EOL, 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'BINARY: EMPTY' . PHP_EOL;
var_dump ($binary->set (''/*empty key*/, 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'BINARY: TOO LONG' . PHP_EOL;
var_dump ($binary->set (str_repeat ('1234567890', 512), 'this is a test'));
var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'BINARY: GET' . PHP_EOL;
// Only newline fails in binary mode (char 10)
for ($i=0;$i<32;$i++) {
	$binary->delete ('binarykeywithnonprintablechar-' . chr($i) . '-here');
	var_dump ($binary->get ('binarykeywithnonprintablechar-' . chr($i) . '-here'));
	var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);
}

echo 'BINARY: SET' . PHP_EOL;
// Only newline fails in binary mode (char 10)
for ($i=0;$i<32;$i++) {
	var_dump ($binary->set ('binarykeywithnonprintablechar-' . chr($i) . '-here', 'this is a test'));
	var_dump ($binary->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);
	$binary->delete ('binarykeywithnonprintablechar-' . chr($i) . '-here');
}

echo 'OK' . PHP_EOL;

--EXPECT--
BINARY: SPACES
bool(true)
bool(true)
BINARY: NEWLINE
bool(false)
bool(true)
BINARY: EMPTY
bool(false)
bool(true)
BINARY: TOO LONG
bool(false)
bool(true)
BINARY: GET
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
BINARY: SET
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
bool(false)
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
OK
