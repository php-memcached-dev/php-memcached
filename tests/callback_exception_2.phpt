--TEST--
Callback initializer throws and dies
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 

function init_cb($m, $id) {
	echo "ran throwing cb\n";
	var_dump($m->isPersistent());
	throw new RuntimeException('Cb exception');
}

function init_cb_die($m, $id) {
	echo "ran quitting cb\n";
	die("quit in cb");
}

error_reporting(0);

echo "cb with exception\n";
try {
	$m1 = new Memcached(null, 'init_cb');
} catch (RuntimeException $e) {
	echo $e->getMessage(), "\n";
}

echo "cb persistent with exception\n";
try {
	$m2 = new Memcached('foo', 'init_cb');
} catch (RuntimeException $e) {
	echo $e->getMessage(), "\n";
}

echo "cb persistent dies\n";
try {
	$m3 = new Memcached('bar', 'init_cb_die');
} catch (RuntimeException $e) {
	echo $e->getMessage(), "\n";
}
echo "not run\n";

--EXPECT--
cb with exception
ran throwing cb
bool(false)
Cb exception
cb persistent with exception
ran throwing cb
bool(true)
Cb exception
cb persistent dies
ran quitting cb
quit in cb
