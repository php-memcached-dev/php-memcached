--TEST--
Memcached::appendByKey()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);
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
