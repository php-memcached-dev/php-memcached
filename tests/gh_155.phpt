--TEST--
Test for bug 155
--SKIPIF--
<?php 
$min_version = "1.4.8";
include dirname(__FILE__) . "/skipif.inc";
// The touch command in binary mode will work in libmemcached 1.0.16, but runs out the timeout clock
// See https://github.com/php-memcached-dev/php-memcached/issues/310 for further explanation
// The problem is fixed fully in libmemcached 1.0.18, so we'll focus tests on that version
if (Memcached::LIBMEMCACHED_VERSION_HEX < 0x01000018) die ('skip too old libmemcached');
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

$m = new Memcached ();

$m->setOption(Memcached::OPT_BINARY_PROTOCOL, true);
$m->addServer(MEMC_SERVER_HOST, MEMC_SERVER_PORT);

$key = 'bug_155_' . uniqid();

$m->set ($key, 'test', time() + 86400);

$m->get ($key);
echo "GET: " . $m->getResultMessage() . PHP_EOL;

$m->touch ($key, time() + 86400);
echo "TOUCH: " . $m->getResultMessage() . PHP_EOL;

$m->touch ($key, time() + 86400);
echo "TOUCH: " . $m->getResultMessage() . PHP_EOL;

$m->get ($key);
echo "GET: " . $m->getResultMessage() . PHP_EOL;

$m->get ($key);
echo "GET: " . $m->getResultMessage() . PHP_EOL;

echo "DONE" . PHP_EOL;

--EXPECT--
GET: SUCCESS
TOUCH: SUCCESS
TOUCH: SUCCESS
GET: SUCCESS
GET: SUCCESS
DONE
