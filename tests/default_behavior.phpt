--TEST--
Default behaviors
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php

$m = new Memcached();
var_dump ($m->getOption(Memcached::OPT_DISTRIBUTION) == Memcached::DISTRIBUTION_MODULA);
var_dump ($m->getOption(Memcached::OPT_BINARY_PROTOCOL) == false);
var_dump ($m->getOption(Memcached::OPT_CONNECT_TIMEOUT) != 0);

ini_set('memcached.default_consistent_hash', true);
ini_set('memcached.default_binary_protocol', true);
ini_set('memcached.default_connect_timeout', 1212);

$m = new Memcached();
var_dump ($m->getOption(Memcached::OPT_DISTRIBUTION) == Memcached::DISTRIBUTION_CONSISTENT);
var_dump ($m->getOption(Memcached::OPT_BINARY_PROTOCOL) == true);
var_dump ($m->getOption(Memcached::OPT_CONNECT_TIMEOUT) == 1212);

echo "OK";

?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
OK
