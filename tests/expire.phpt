--TEST--
Memcached store, fetch & touch expired key
--XFAIL--
libmemcached ignores expiration with binary proto
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

function run_expiry_test ($m) {
	
	$key = uniqid ('will_expire_');
	
	$set = $m->set($key, "foo", 2);
	$v = $m->get($key);
	if (!$set || $v != 'foo') {
		echo "Error setting key to \"foo\" with 2s expiry.\n";
		return;
	}

	sleep(1);
	$res = $m->touch($key, 2);
	$v = $m->get($key);

	if(!$res || $v != 'foo') {
		echo "Error touching key for another 2s expiry.\n";
		var_dump($res);
		var_dump($m->getResultMessage());
		var_dump($v);
		return;
	}

	sleep(3);
	$v = $m->get($key);

	if ($v !== Memcached::GET_ERROR_RETURN_VALUE) {
		echo "Wanted:\n";
		var_dump(Memcached::GET_ERROR_RETURN_VALUE);
		echo "from get of expired value. Got:\n";
		var_dump($v);
		return;
	}
	echo "All OK" . PHP_EOL;
}


$m = new Memcached();
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$m->addServer('127.0.0.1', 11211, 1);

echo '-- binary protocol' . PHP_EOL;
run_expiry_test ($m);

$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

echo '-- text protocol' . PHP_EOL;
run_expiry_test ($m);

echo "DONE TEST\n";
?>
--EXPECT--
-- binary protocol
All OK
-- text protocol
All OK
DONE TEST
