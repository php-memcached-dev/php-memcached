--TEST--
Memcached store, fetch & touch expired key
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$m->addServer('127.0.0.1', 11211, 1);

$set = $m->set('will_expire', "foo", 2);
$v = $m->get('will_expire');
if (!$set || $v != 'foo') {
	echo "Error setting will_expire to \"foo\" with 2s expiry.\n";
}
sleep(1);
$res = $m->touch('will_expire', 2);
$v = $m->get('will_expire');
if(!$res || $v != 'foo') {
  echo "Error touching will_expire for another 2s expiry.\n";
  var_dump($res);
  var_dump($m->getResultMessage());
  var_dump($v);
}

sleep(3);
$v = $m->get('will_expire');

if ($v !== Memcached::GET_ERROR_RETURN_VALUE) {
	echo "Wanted:\n";
	var_dump(Memcached::GET_ERROR_RETURN_VALUE);
	echo "from get of expired value. Got:\n";
	var_dump($v);
}

// test with plaintext proto should throw error
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$set = $m->set('will_expire', "foo", 2);
$v = $m->touch('will_expire');
if($v !== false) {
  echo "Touch with text protocol should return false.\n";
}

echo "OK\n";
?>
--EXPECTF--
Warning: Memcached::touch(): touch is only supported with binary protocol in %s on line %d
OK
