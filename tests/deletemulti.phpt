--TEST--
Delete multi
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

function has_all_keys($keys, $array, $check_true = false)
{
	foreach ($keys as $key) {
		if (!isset($array[$key]))
			return false;
		
		if ($check_true && $array[$key] !== true)
			return false;
	}
	return true;
}

$data = array(
	'foo' => 'foo-data',
	'bar' => 'bar-data',
	'baz' => 'baz-data',
	'lol' => 'lol-data',
	'kek' => 'kek-data',
);

$keys = array_keys($data);

$null = null;
$m->setMulti($data, 3600);

/* Check that all keys were stored */
var_dump(has_all_keys($keys, $m->getMulti($keys)));

/* Check that all keys get deleted */
$deleted = $m->deleteMulti($keys);
var_dump(has_all_keys($keys, $deleted, true));

/* Try to get the deleted keys, should give empty array */
var_dump($m->getMulti($keys));

/* ---- same tests for byKey variants ---- */
$m->setMultiByKey("hi", $data, 3600);

var_dump(has_all_keys($keys, $m->getMultiByKey('hi', $keys)));

/* Check that all keys get deleted */
$deleted = $m->deleteMultiByKey('hi', $keys);
var_dump(has_all_keys($keys, $deleted, true));

/* Try to get the deleted keys, should give empty array */
var_dump($m->getMultiByKey('hi', $keys));

/* Test deleting non-existent keys */
$keys = array();
$keys[] = "nothere";
$keys[] = "nothere2";

$retval = $m->deleteMulti($keys);

foreach ($retval as $key => $value) {
    if ($value === Memcached::RES_NOTFOUND) {
        echo "$key NOT FOUND\n";
    }
}


?>
--EXPECT--
bool(true)
bool(true)
array(0) {
}
bool(true)
bool(true)
array(0) {
}
nothere NOT FOUND
nothere2 NOT FOUND