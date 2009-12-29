--TEST--
Memcached options
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$m = new Memcached();
$m->setOption(Memcached::OPT_SERIALIZER, Memcached::SERIALIZER_PHP);

var_dump($m->getOption(Memcached::OPT_COMPRESSION));
var_dump($m->getOption(Memcached::OPT_SERIALIZER));
var_dump($m->getOption(Memcached::OPT_SOCKET_SEND_SIZE));

$m->setOption(Memcached::OPT_PREFIX_KEY, "\x01");

var_dump($m->getOption(Memcached::OPT_HASH) == Memcached::HASH_DEFAULT);
$m->setOption(Memcached::OPT_HASH, Memcached::HASH_MURMUR);
var_dump($m->getOption(Memcached::OPT_HASH) == Memcached::HASH_MURMUR);


$m->setOption(Memcached::OPT_COMPRESSION_TYPE, Memcached::COMPRESSION_ZLIB);
var_dump($m->getOption(Memcached::OPT_COMPRESSION_TYPE) == Memcached::COMPRESSION_ZLIB);

$m->setOption(Memcached::OPT_COMPRESSION_TYPE, Memcached::COMPRESSION_FASTLZ);
var_dump($m->getOption(Memcached::OPT_COMPRESSION_TYPE) == Memcached::COMPRESSION_FASTLZ);

var_dump($m->setOption(Memcached::OPT_COMPRESSION_TYPE, 0));
var_dump($m->getOption(Memcached::OPT_COMPRESSION_TYPE) == Memcached::COMPRESSION_FASTLZ);
?>
--EXPECTF--
bool(true)
int(1)

Warning: Memcached::getOption(): no servers defined in %s on line %d
NULL

Warning: Memcached::setOption(): bad key provided in %s on line %d
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
