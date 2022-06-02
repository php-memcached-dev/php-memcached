--TEST--
Memcached options
--SKIPIF--
<?php include "skipif.inc";?>
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

echo "item_size_limit setOption\n";
var_dump($m->setOption(Memcached::OPT_ITEM_SIZE_LIMIT, 0));
var_dump($m->getOption(Memcached::OPT_ITEM_SIZE_LIMIT) === 0);
var_dump($m->setOption(Memcached::OPT_ITEM_SIZE_LIMIT, -1));
var_dump($m->setOption(Memcached::OPT_ITEM_SIZE_LIMIT, 1000000));
var_dump($m->getOption(Memcached::OPT_ITEM_SIZE_LIMIT) == 1000000);

echo "item_size_limit ini\n";
ini_set('memcached.item_size_limit', '0');
$m = new Memcached();
var_dump($m->getOption(Memcached::OPT_ITEM_SIZE_LIMIT) === 0);

ini_set('memcached.item_size_limit', '1000000');
$m = new Memcached();
var_dump($m->getOption(Memcached::OPT_ITEM_SIZE_LIMIT) == 1000000);

ini_set('memcached.item_size_limit', null);
$m = new Memcached();
var_dump($m->getOption(Memcached::OPT_ITEM_SIZE_LIMIT) === 0);
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
item_size_limit setOption
bool(true)
bool(true)

Warning: Memcached::setOption(): ITEM_SIZE_LIMIT must be >= 0 in %s on line %d
bool(false)
bool(true)
bool(true)
item_size_limit ini
bool(true)
bool(true)
bool(true)
