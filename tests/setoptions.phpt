--TEST--
Set options using setOptions
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$m = new Memcached();
var_dump($m->setOptions(array(
	Memcached::OPT_PREFIX_KEY => 'a_prefix',
	Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHP,
	Memcached::OPT_COMPRESSION => 0,
	101 => 2,
)));

var_dump($m->getOption(Memcached::OPT_PREFIX_KEY) == 'a_prefix');
var_dump($m->getOption(Memcached::OPT_SERIALIZER) == Memcached::SERIALIZER_PHP);
var_dump($m->getOption(Memcached::OPT_COMPRESSION) == 0);

// XXX: this should return something else than int(0)?
var_dump($m->getOption(101));


--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
int(0)
