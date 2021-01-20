--TEST--
Memcached::get() with cache callback
--SKIPIF--
<?php
if (!extension_loaded("memcached")) {
	die("skip memcached is not loaded\n");
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

var_dump($cache->getStats());

$cache->quit();

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
bool(false)
Done
