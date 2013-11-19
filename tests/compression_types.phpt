--TEST--
Memcached compression test
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$data = file_get_contents(dirname(__FILE__) . '/testdata.res');

function get_compression($name) {
	switch (strtolower($name)) {
		case 'zlib':
			return Memcached::COMPRESSION_ZLIB;
		case 'fastlz':
			return Memcached::COMPRESSION_FASTLZ;
		default:
			echo "Strange compression type: $name\n";
			return 0;
	}
}

function fetch_with_compression($m, $key, $value, $set_compression = '', $get_compression = '') {
	
	echo "set=[$set_compression] get=[$get_compression]\n";
	
	if (!$set_compression) {
		$m->setOption(Memcached::OPT_COMPRESSION, false);
	} else {
		$m->setOption(Memcached::OPT_COMPRESSION, true);
		$m->setOption(Memcached::OPT_COMPRESSION_TYPE, get_compression($set_compression));
	}
	
	$m->set($key, $value, 1800);
	
	if (!$get_compression) {
		$m->setOption(Memcached::OPT_COMPRESSION, true);
	} else {
		$m->setOption(Memcached::OPT_COMPRESSION, true);
		$m->setOption(Memcached::OPT_COMPRESSION_TYPE, get_compression($get_compression));
	}

	$value_back = $m->get($key);
	var_dump($value === $value_back);
}

fetch_with_compression($m, 'hello1', $data, 'zlib', 'zlib');
fetch_with_compression($m, 'hello2', $data, 'zlib', 'fastlz');
fetch_with_compression($m, 'hello3', $data, 'fastlz', 'fastlz');
fetch_with_compression($m, 'hello4', $data, 'fastlz', 'zlib');
fetch_with_compression($m, 'hello5', $data, '', 'zlib');
fetch_with_compression($m, 'hello6', $data, '', 'fastlz');
fetch_with_compression($m, 'hello7', $data, 'zlib', '');
fetch_with_compression($m, 'hello8', $data, 'fastlz', '');
fetch_with_compression($m, 'hello9', $data, '', '');
?>
--EXPECT--
set=[zlib] get=[zlib]
bool(true)
set=[zlib] get=[fastlz]
bool(true)
set=[fastlz] get=[fastlz]
bool(true)
set=[fastlz] get=[zlib]
bool(true)
set=[] get=[zlib]
bool(true)
set=[] get=[fastlz]
bool(true)
set=[zlib] get=[]
bool(true)
set=[fastlz] get=[]
bool(true)
set=[] get=[]
bool(true)
