--TEST--
Memcached store & fetch expired key
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$set = $m->set('will_expire', "foo", 2);
$v = $m->get('will_expire');
if (!$set || $v != 'foo') {
	echo "Error setting will_expire to \"foo\" with 2s expiry.\n";
}
sleep(3);
$v = $m->get('will_expire');

if ($v !== Memcached::GET_ERROR_RETURN_VALUE) {
	echo "Wanted:\n";
	var_dump(Memcached::GET_ERROR_RETURN_VALUE);
	echo "from get of expired value. Got:\n";
	var_dump($v);
}
?>
--EXPECT--
