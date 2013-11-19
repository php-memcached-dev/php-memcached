--TEST--
Memcached fetch cas & set cas
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$m->delete('cas_test');
$cas_token = null;

$m->set('cas_test', 10);
$v = $m->get('cas_test', null, $cas_token);

if (is_null($cas_token)) {
	echo "Null cas token for key: cas_test value: 10\n";
	return;
}

$v = $m->cas($cas_token, 'cas_test', 11);
if (!$v) {
	echo "Error setting key: cas_test value: 11 with CAS: $cas_token\n";
	return;
}

$v = $m->get('cas_test');

if ($v !== 11) {
	echo "Wanted cas_test to be 11, value is: ";
	var_dump($v);
}

$v = $m->get('cas_test', null, 2);
if ($v != 11) {
	echo "Failed to get the value with \$cas_token passed by value (2)\n";
	return;
}

$v = $m->get('cas_test', null, null);
if ($v != 11) {
	echo "Failed to get the value with \$cas_token passed by value (null)\n";
	return;
}

$v = $m->get('cas_test', null, $data = array(2, 4));
if ($v != 11 || $data !== array(2, 4)) {
	echo "Failed to get the value with \$cas_token passed by value (\$data = array(2, 4))\n";
	return;
}

echo "OK\n";
?>
--EXPECT--
OK