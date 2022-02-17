--TEST--
Test for Github issue 500
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

$m = new Memcached();
$newServers = array(
                  array(MEMC_SERVER_HOST, MEMC_SERVER_PORT),
              );            
$m->addServers($newServers);

$m->set('floatpoint', 100.200);
$n = $m->get('floatpoint');
var_dump($n);

$m->set('floatpoint_neg', -300.400);
$n = $m->get('floatpoint_neg');
var_dump($n);
?>
--EXPECT--
float(100.200)
float(-300.400)
