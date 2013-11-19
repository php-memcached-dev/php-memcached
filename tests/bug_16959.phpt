--TEST--
Memcached: Bug #16959 (getMulti + BINARY_PROTOCOL problem)
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$cache = memc_get_instance (array (
							Memcached::OPT_BINARY_PROTOCOL => true
						));

$cache->set('key_0', 'value0');
$cache->set('key_0_additional', 'value0_additional');

// -------------- NORMAL
echo "NORMAL\n";
$keys = array( 'key_0', 'key_0_additional' );
$values = $cache->getMulti($keys);
echo $cache->getResultMessage(), "\n";
echo "Values:\n";
foreach ($values as $k => $v) {
	var_dump($k);
	var_dump($v);
	var_dump($values[$k]);
}
// --------------- REVERSED KEY ORDER
echo "REVERSED KEY ORDER\n";
$keys = array( 'key_0_additional', 'key_0' );
$values = $cache->getMulti($keys);
echo $cache->getResultMessage(), "\n";
echo "Values:\n";
foreach ($values as $k => $v) {
	var_dump($k);
	var_dump($v);
	var_dump($values[$k]);
}

--EXPECT--
NORMAL
SUCCESS
Values:
string(5) "key_0"
string(6) "value0"
string(6) "value0"
string(16) "key_0_additional"
string(17) "value0_additional"
string(17) "value0_additional"
REVERSED KEY ORDER
SUCCESS
Values:
string(16) "key_0_additional"
string(17) "value0_additional"
string(17) "value0_additional"
string(5) "key_0"
string(6) "value0"
string(6) "value0"
