--TEST--
Change prefix, pecl bug #17137
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$memcache = new Memcached();
$memcache->addServer('127.0.0.1', 11211);

$memcache->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$memcache->setOption(Memcached::OPT_PREFIX_KEY, 'prefix1');
var_dump($memcache->getOption(Memcached::OPT_PREFIX_KEY));

var_dump($memcache->set('test', "val_prefix1", 120));
var_dump($memcache->get('test'));

$memcache2 = new Memcached();
$memcache2->addServer('127.0.0.1', 11211);
$memcache2->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$memcache2->setOption(Memcached::OPT_PREFIX_KEY, 'prefix2');
var_dump($memcache2->getOption(Memcached::OPT_PREFIX_KEY));

var_dump($memcache2->set('test', "val_prefix2", 120));
var_dump($memcache2->get('test'));


var_dump($memcache->get('test'));
--EXPECT--
string(7) "prefix1"
bool(true)
string(11) "val_prefix1"
string(7) "prefix2"
bool(true)
string(11) "val_prefix2"
string(11) "val_prefix1"
