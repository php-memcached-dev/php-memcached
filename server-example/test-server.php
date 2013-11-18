<?php

$cache = new Memcached();
$cache->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$cache->setOption(Memcached::OPT_COMPRESSION, false);
$cache->addServer('localhost', 3434);

$cache->add("add_key", "hello", 500);
$cache->append("append_key", "world");
$cache->prepend("prepend_key", "world");

$cache->increment("incr", 2, 1, 500);
$cache->decrement("decr", 2, 1, 500);

$cache->delete("delete_k");
$cache->flush(1);

var_dump ($cache->get ('get_this'));

$cache->set ('set_key', 'value 1', 100);
$cache->replace ('replace_key', 'value 2', 200);

$cache->getStats ();

$cache->quit();
sleep (1);