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

$m->delete('cas_e2big_test');

$m->set('cas_e2big_test', 'hello');
$result = $m->get('cas_e2big_test', null, Memcached::GET_EXTENDED);
var_dump(is_array($result) && isset($result['cas']) && isset($result['value']) && $result['value'] == 'hello');

$value = str_repeat('a large payload', 1024 * 1024);

var_dump($m->cas($result['cas'], 'cas_e2big_test', $value, 360));
var_dump($m->getResultCode() == Memcached::RES_E2BIG);
var_dump($m->getResultMessage() == 'ITEM TOO BIG');
var_dump($m->get('cas_e2big_test') == 'hello');
var_dump($m->getResultCode() == Memcached::RES_SUCCESS);
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
