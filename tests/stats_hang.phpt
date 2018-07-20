--TEST--
Check stats does not hang on non-blocking binary protocol
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$key = MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT;

// Both options set means we have to reconnect to get stats
$m->setOption(Memcached::OPT_NO_BLOCK, true);
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);

$stats = $m->getStats();
$conns1 = $stats[$key]['total_connections'];

$stats = $m->getStats();
$conns2 = $stats[$key]['total_connections'];

var_dump($conns1 == $conns2);
var_dump($m->getOption(Memcached::OPT_NO_BLOCK));
var_dump($m->getOption(Memcached::OPT_BINARY_PROTOCOL));
echo "OK" . PHP_EOL;

// If either or both options are false no need to reconnect
$m->setOption(Memcached::OPT_NO_BLOCK, false);
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);

$stats = $m->getStats();
$conns1 = $stats[$key]['total_connections'];

$stats = $m->getStats();
$conns2 = $stats[$key]['total_connections'];

var_dump($conns1 == $conns2);
var_dump($m->getOption(Memcached::OPT_NO_BLOCK));
var_dump($m->getOption(Memcached::OPT_BINARY_PROTOCOL));
echo "OK" . PHP_EOL;

// If either or both options are false no need to reconnect
$m->setOption(Memcached::OPT_NO_BLOCK, true);
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, false);

$stats = $m->getStats();
$conns1 = $stats[$key]['total_connections'];

$stats = $m->getStats();
$conns2 = $stats[$key]['total_connections'];

var_dump($conns1 == $conns2);
var_dump($m->getOption(Memcached::OPT_NO_BLOCK));
var_dump($m->getOption(Memcached::OPT_BINARY_PROTOCOL));
echo "OK" . PHP_EOL;

// If either or both options are false no need to reconnect
$m->setOption(Memcached::OPT_NO_BLOCK, false);
$m->setOption(Memcached::OPT_BINARY_PROTOCOL, false);

$stats = $m->getStats();
$conns1 = $stats[$key]['total_connections'];

$stats = $m->getStats();
$conns2 = $stats[$key]['total_connections'];

var_dump($conns1 == $conns2);
var_dump($m->getOption(Memcached::OPT_NO_BLOCK));
var_dump($m->getOption(Memcached::OPT_BINARY_PROTOCOL));
echo "OK" . PHP_EOL;

?>
--EXPECT--
bool(false)
int(1)
int(1)
OK
bool(true)
int(0)
int(1)
OK
bool(true)
int(1)
int(0)
OK
bool(true)
int(0)
int(0)
OK
