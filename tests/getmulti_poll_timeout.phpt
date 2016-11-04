--TEST--
Memcached::getMulti() poll timeout
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance(array(Memcached::OPT_POLL_TIMEOUT => 1));

var_dump($m->getOption(Memcached::OPT_POLL_TIMEOUT));

$keys = array();
for ($i = 0; $i < 100000; $i++) {
	$keys[] = $i;
	$m->set($i, $i, 3600);
}

$v = $m->getMulti($keys);

var_dump($m->getResultCode() != Memcached::RES_SUCCESS);

--EXPECT--
int(1)
bool(true)
