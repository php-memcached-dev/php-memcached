--TEST--
Memcached::casByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();

$m->delete('cas_test');

$m->setByKey('keffe', 'cas_test', 10);
$v = $m->getbyKey('keffe', 'cas_test', null, Memcached::GET_EXTENDED);

$cas_token = $v["cas"];
if (empty($cas_token)) {
	echo "Null cas token for key: cas_test value: 10\n";
	return;
}

$v = $m->casByKey($cas_token, 'keffe', 'cas_test', 11);
if (!$v) {
	echo "Error setting key: cas_test value: 11 with CAS: $cas_token\n";
	return;
}

$v = $m->getByKey('keffe', 'cas_test');

if ($v !== 11) {
	echo "Wanted cas_test to be 11, value is: ";
	var_dump($v);
}
?>
--EXPECT--
