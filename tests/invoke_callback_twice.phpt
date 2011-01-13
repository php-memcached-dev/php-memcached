--TEST--
Test that callback is invoked on new object only once
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

function my_func(Memcached $obj, $persistent_id = null)
{
	echo "Invoked for '{$persistent_id}'\n";
}

$m1 = new Memcached('foobar', 'my_func');
$m2 = new Memcached('foobar', 'my_func');

echo "OK\n";

--EXPECT--
Invoked for 'foobar'
OK
