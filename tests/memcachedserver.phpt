--TEST--
MemcachedServer
--SKIPIF--
<?php
if (!extension_loaded("memcached")) {
	die("skip memcached is not loaded\n");
}
if (!class_exists("MemcachedServer")) {
	die("skip memcached not built with libmemcachedprotocol support\n");
}

if (Memcached::LIBMEMCACHED_VERSION_HEX < 0x1001000) {
       die("skip needs at least libmemcached 1.1.0\n");
}
?>
--FILE--
<?php
include __DIR__ . '/server.inc';
$server = memcached_server_start();

$cache = new Memcached();
$cache->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$cache->setOption(Memcached::OPT_COMPRESSION, false);
$cache->addServer('127.0.0.1', 3434);

$cache->add("add_key", "hello", 500);
$cache->append("append_key", "world");
$cache->prepend("prepend_key", "world");

$cache->increment("incr", 2, 1, 500);
$cache->decrement("decr", 2, 1, 500);

$cache->delete("delete_k");
$cache->flush(1);

var_dump($cache->get('get_this'));

$cache->set ('set_key', 'value 1', 100);
$cache->replace ('replace_key', 'value 2', 200);

var_dump($cache->getVersion());
var_dump($cache->getStats());
var_dump($cache->getStats("empty"));
var_dump($cache->getStats("foobar"));
var_dump($cache->getStats("scalar"));
var_dump($cache->getStats("numeric array"));

$cache->quit();
usleep(50000);

memcached_server_stop($server);
?>
Done
--EXPECTF--
Listening on 127.0.0.1:3434
Incoming connection from 127.0.0.1:%s
Incoming connection from 127.0.0.1:%s
client_id=[%s]: Add key=[add_key], value=[hello], flags=[0], expiration=[500]
client_id=[%s]: Append key=[append_key], value=[world], cas=[0]
client_id=[%s]: Prepend key=[prepend_key], value=[world], cas=[0]
client_id=[%s]: Incrementing key=[incr], delta=[2], initial=[1], expiration=[500]
client_id=[%s]: Decrementing key=[decr], delta=[2], initial=[1], expiration=[500]
client_id=[%s]: Delete key=[delete_k], cas=[0]
client_id=[%s]: Flush when=[1]
client_id=[%s]: Get key=[get_this]
client_id=[%s]: Noop
string(20) "Hello to you client!"
client_id=[%s]: Set key=[set_key], value=[value 1], flags=[0], expiration=[100], cas=[0]
client_id=[%s]: Replace key=[replace_key], value=[value 2], flags=[0], expiration=[200], cas=[0]
client_id=[%s]: Version
array(1) {
  ["127.0.0.1:3434"]=>
  string(5) "1.1.1"
}
client_id=[%s]: Stat key=[]
array(1) {
  ["127.0.0.1:3434"]=>
  array(2) {
    ["key"]=>
    string(0) ""
    ["foo"]=>
    string(3) "bar"
  }
}
client_id=[%s]: Stat key=[empty]
array(0) {
}
client_id=[%s]: Stat key=[foobar]
array(1) {
  ["127.0.0.1:3434"]=>
  array(2) {
    ["key"]=>
    string(6) "foobar"
    ["foo"]=>
    string(3) "bar"
  }
}
client_id=[%s]: Stat key=[scalar]
array(1) {
  ["127.0.0.1:3434"]=>
  array(1) {
    [0]=>
    string(%d) "you want it, you get it"
  }
}
client_id=[%s]: Stat key=[numeric array]
array(1) {
  ["127.0.0.1:3434"]=>
  array(3) {
    [-1]=>
    string(3) "one"
    [0]=>
    string(3) "two"
    [1]=>
    string(5) "three"
  }
}
client_id=[%s]: Client quit
Done
