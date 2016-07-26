--TEST--
make sure that callback exception behaves correctly
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

function throw_something(Memcached $object, $p_id = null)
{
    throw new Exception("from cb");
}

function empty_cb(Memcached $object, $p_id = null)
{
    echo "empty_cb called\n";
}

try {
    $m = new Memcached('test_id', 'throw_something');
    echo "fail\n";
} catch (Exception $e) {
    echo "success\n";
}

try {
    $m = new Memcached('test_id', 'throw_something');
    echo "fail\n";
} catch (Exception $e) {
    echo "success\n";
}

try {
    $m = new Memcached('test_id', 'empty_cb');
    $m = new Memcached('test_id', 'empty_cb');
} catch (Exception $e) {
    echo "fail\n";
}

try {
    $m = new Memcached(null, 'throw_something');
    echo "fail\n";
} catch (Exception $e) {
    echo "success\n";
}

echo "OK\n";

?>
--EXPECT--
success
success
empty_cb called
success
OK
