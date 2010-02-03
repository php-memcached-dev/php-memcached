--TEST--
Memcached::addServer() unix doamin socket
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--CLEAN--
<?php
unlink('/tmp/memc_test_unix_socket');
--FILE--
<?php
$fh = stream_socket_server('unix:///tmp/memc_test_unix_socket');

$m = new Memcached();
$m->setOption(Memcached::OPT_CONNECT_TIMEOUT, 1);
$m->setOption(Memcached::OPT_SEND_TIMEOUT, 1);
$m->setOption(Memcached::OPT_RECV_TIMEOUT, 1);
$m->addServer('/tmp/memc_test_unix_socket', 11211, 1);

--EXPECT--
