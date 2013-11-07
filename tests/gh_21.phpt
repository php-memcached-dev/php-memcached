--TEST--
Test for Github issue 21
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$newServers = array(
                  array('127.0.0.1', 11211),
              );            
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);

$m->addServers($newServers);

$d = $m->get('foo');

$m->set('counter', 5);
$n = $m->decrement('counter');
var_dump($n);

$n = $m->decrement('counter', 10);
var_dump($n);

var_dump($m->get('counter'));

$m->set('counter', 'abc');
$n = $m->increment('counter');
var_dump($n);
?>
--EXPECT--
int(4)
int(0)
int(0)
bool(false)