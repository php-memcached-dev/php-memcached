--TEST--
Compressed item with inconsistent length header (fastlz, zlib)
--SKIPIF--
<?php include "skipif.inc";?>
--INI--
memory_limit=128M
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
include dirname(__FILE__) . '/compression_length.inc';

$m = memc_get_instance(array(Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHP));
$value = str_repeat('abcdefgh', 500);

memc_compressed_length_test($m, 'fastlz', Memcached::COMPRESSION_FASTLZ, $value);
memc_compressed_length_test($m, 'zlib', Memcached::COMPRESSION_ZLIB, $value);
?>
--EXPECT--
fastlz unchanged: original value
fastlz larger by 4096: original value
fastlz smaller by 1: false (could not decompress value)
fastlz 1 GiB: false (could not decompress value: stored length does not match the compressed data)
fastlz 4 GiB - 1: false (could not decompress value: stored length does not match the compressed data)
zlib unchanged: original value
zlib larger by 4096: original value
zlib smaller by 1: false (could not decompress value)
zlib 1 GiB: false (could not decompress value: stored length does not match the compressed data)
zlib 4 GiB - 1: false (could not decompress value: stored length does not match the compressed data)
