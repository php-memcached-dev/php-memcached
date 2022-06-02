--TEST--
set data exceeding size limit
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (
	Memcached::OPT_ITEM_SIZE_LIMIT => 100,
));

$m->delete('set_large_e2big_test');

$value = str_repeat('a large payload', 1024 * 1024);

var_dump($m->set('set_large_e2big_test', $value));
var_dump($m->getResultCode() == Memcached::RES_E2BIG);
var_dump($m->getResultMessage() == 'ITEM TOO BIG');
var_dump($m->get('set_large_e2big_test') === false);
var_dump($m->getResultCode() == Memcached::RES_NOTFOUND);
?>
--EXPECT--
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
