--TEST--
Test for bug 155
--SKIPIF--
<?php 
if (!extension_loaded("memcached"))
	print "skip";
if (Memcached::LIBMEMCACHED_VERSION_HEX < 0x01000016) die ('skip too old libmemcached');
?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';

$m = new Memcached ();

$m->setOption(Memcached::OPT_BINARY_PROTOCOL,true);
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
