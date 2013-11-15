<?php

$cache = new Memcached();
$cache->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$cache->setOption(Memcached::OPT_COMPRESSION, false);
$cache->addServer('localhost', 3434);

$cache->set ('set_key1', 'This is the first key', 10);
var_dump ($cache->get ('set_key1'));

$cache->set ('set_key2', 'This is the second key', 2);
var_dump ($cache->get ('set_key2'));
