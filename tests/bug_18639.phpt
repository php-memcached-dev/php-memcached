--TEST--
Memcached::getServerByKey(): Bug pecl#18639 (Segfault in getServerByKey)
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

var_dump($m->set('test', 'test1'));
$array = $m->getServerByKey('1');
var_dump( $array["host"] == MEMC_SERVER_HOST ); 
var_dump( $array["port"] == MEMC_SERVER_PORT ); 
var_dump ( $array["weight"] ); 

--EXPECTF--
bool(true)
bool(true)
bool(true)
int(%r[01]%r) 
