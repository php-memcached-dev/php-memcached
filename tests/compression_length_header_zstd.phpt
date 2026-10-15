--TEST--
Compressed item with inconsistent length header (zstd)
--SKIPIF--
<?php
include "skipif.inc";
if (!Memcached::HAVE_ZSTD) die("skip zstd support not enabled\n");
?>
--INI--
memory_limit=128M
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
include dirname(__FILE__) . '/compression_length.inc';

$m = memc_get_instance(array(Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHP));
$value = str_repeat('abcdefgh', 500);

memc_compressed_length_test($m, 'zstd', Memcached::COMPRESSION_ZSTD, $value);
?>
--EXPECT--
zstd unchanged: original value
zstd larger by 4096: false (could not decompress value: stored length does not match the compressed data)
zstd smaller by 1: false (could not decompress value: stored length does not match the compressed data)
zstd 1 GiB: false (could not decompress value: stored length does not match the compressed data)
zstd 4 GiB - 1: false (could not decompress value: stored length does not match the compressed data)
