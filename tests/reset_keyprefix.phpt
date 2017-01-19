--TEST--
Cannot reset OPT_PREFIX_KEY #293
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

$m = memc_get_instance ();

$m->set('key1', 'abc');
var_dump($m->get('key1'));

$m->setOption(Memcached::OPT_PREFIX_KEY, 'prefix');
var_dump($m->get('key1'));

$m->setOption(Memcached::OPT_PREFIX_KEY, false);
var_dump($m->get('key1'));

$m->setOption(Memcached::OPT_PREFIX_KEY, 'prefix');
var_dump($m->get('key1'));

$m->setOption(Memcached::OPT_PREFIX_KEY, '');
var_dump($m->get('key1'));

$m->setOption(Memcached::OPT_PREFIX_KEY, 'prefix');
var_dump($m->get('key1'));

$m->setOption(Memcached::OPT_PREFIX_KEY, null);
var_dump($m->get('key1'));
--EXPECTF--
string(3) "abc"
bool(false)
string(3) "abc"
bool(false)
string(3) "abc"
bool(false)
string(3) "abc"