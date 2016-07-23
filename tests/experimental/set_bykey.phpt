--TEST--
Memcached::setByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

var_dump($m->setByKey('foo', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->setByKey('', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->setByKey('foo', '', 1, 10));
echo $m->getResultMessage(), "\n";
// This is OK for the binary protocol
$rv = $m->setByKey('foo', ' asd едц', 1, 1);
if ($m->getOption(Memcached::OPT_BINARY_PROTOCOL)) {
	if ($rv !== true and $m->getResultCode() !== Memcached::RES_SUCCESS) {
		var_dump($rv);
		echo $m->getResultMessage(), "\n";
	}
} else {
	if ($rv !== false or $m->getResultCode() === Memcached::RES_SUCCESS) {
		var_dump($rv);
		echo $m->getResultMessage(), "\n";
	}
}

--EXPECT--
bool(true)
SUCCESS
bool(true)
SUCCESS
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
