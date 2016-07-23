--TEST--
Memcached::appendByKey()
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . '/config.inc';
$m = memc_get_instance ();
$m->setOption(Memcached::OPT_COMPRESSION, false);

var_dump($m->setByKey('foo', 'foo', 'bar', 10));
echo $m->getResultMessage(), "\n";
var_dump($m->prependByKey('foo', 'foo', 'baz'));
echo $m->getResultMessage(), "\n";
var_dump($m->getByKey('foo', 'foo'));

--EXPECT--
bool(true)
SUCCESS
bool(true)
SUCCESS
string(6) "bazbar"
