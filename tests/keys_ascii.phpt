--TEST--
Test valid and invalid keys - ascii
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$ascii = memc_get_instance (array (
				Memcached::OPT_BINARY_PROTOCOL => false,
				Memcached::OPT_VERIFY_KEY => false
		));
// libmemcached can verify keys, but these are tests are for our own
// function s_memc_valid_key_ascii, so explicitly disable the checks
// that libmemcached can perform.

echo 'ASCII: SPACES' . PHP_EOL;
var_dump ($ascii->set ('ascii key with spaces', 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'ASCII: NEWLINE' . PHP_EOL;
var_dump ($ascii->set ('asciikeywithnewline' . PHP_EOL, 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'ASCII: EMPTY' . PHP_EOL;
var_dump ($ascii->set (''/*empty key*/, 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'ASCII: TOO LONG' . PHP_EOL;
var_dump ($ascii->set (str_repeat ('1234567890', 512), 'this is a test'));
var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);

echo 'ASCII: GET' . PHP_EOL;
for ($i=0;$i<32;$i++) {
	var_dump ($ascii->get ('asciikeywithnonprintablechar-' . chr($i) . '-here'));
	var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);
}

echo 'ASCII: SET' . PHP_EOL;
for ($i=0;$i<32;$i++) {
	var_dump ($ascii->set ('asciikeywithnonprintablechar-' . chr($i) . '-here', 'this is a test'));
	var_dump ($ascii->getResultCode () == Memcached::RES_BAD_KEY_PROVIDED);
}

echo 'OK' . PHP_EOL;

--EXPECT--
ASCII: SPACES
bool(false)
bool(true)
ASCII: NEWLINE
bool(false)
bool(true)
ASCII: EMPTY
bool(false)
bool(true)
ASCII: TOO LONG
bool(false)
bool(true)
ASCII: GET
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
ASCII: SET
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
