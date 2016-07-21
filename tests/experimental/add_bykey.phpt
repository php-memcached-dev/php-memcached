--TEST--
Memcached::addByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

$m->delete('foo');
var_dump($m->addByKey('foo', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->addByKey('', 'foo', 1, 10));
echo $m->getResultMessage(), "\n";
var_dump($m->addByKey('foo', '', 1, 10));
echo $m->getResultMessage(), "\n";
// This is OK for the binary protocol
$rv = $m->addByKey('foo', ' asd едц', 1, 1);
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

--EXPECTF--
bool(true)
SUCCESS
bool(false)
%rNOT STORED|CONNECTION DATA EXISTS%r
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
