--TEST--
Change prefix, pecl bug #17137
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

include dirname (__FILE__) . '/config.inc';
$memcache = memc_get_instance (array (
								Memcached::OPT_BINARY_PROTOCOL => true,
								Memcached::OPT_PREFIX_KEY => 'prefix1',
							));

$memcache2 = memc_get_instance (array (
								Memcached::OPT_BINARY_PROTOCOL => true,
								Memcached::OPT_PREFIX_KEY => 'prefix2',
							));

var_dump($memcache->getOption(Memcached::OPT_PREFIX_KEY));

var_dump($memcache->set('test', "val_prefix1", 120));
var_dump($memcache->get('test'));


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
