--TEST--
Memcached compression test
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance ();

$short_data = "abcdefg";
$data = file_get_contents(dirname(__FILE__) . '/testdata.res');

set_error_handler(function($errno, $errstr, $errfile, $errline, array $errcontext) {
	echo "$errstr\n";
	return true;
}, E_WARNING);

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

function fetch_with_compression($m, $key, $value, $set_compression = '', $factor = 1.3, $threshold = 2000) {
 	ini_set("memcached.compression_factor", $factor);
 	ini_set("memcached.compression_threshold", $threshold);
	
	$len=strlen($value);
	echo "len=[$len] set=[$set_compression] factor=[$factor] threshold=[$threshold]\n";
	
	if (!$set_compression) {
		$m->setOption(Memcached::OPT_COMPRESSION, false);
	} else {
		$m->setOption(Memcached::OPT_COMPRESSION, true);
		$m->setOption(Memcached::OPT_COMPRESSION_TYPE, get_compression($set_compression));
	}
	
	$m->set($key, $value, 1800);
	
	$value_back = $m->get($key);
	var_dump($value === $value_back);
}

fetch_with_compression($m, 'hello01', $data, 'zlib',         1.3, 4);
fetch_with_compression($m, 'hello02', $data, 'fastlz',       1.3, 4);
fetch_with_compression($m, 'hello03', $data, '',             1.3, 4);
fetch_with_compression($m, 'hello04', $short_data, 'zlib',   1.3, 4);
fetch_with_compression($m, 'hello05', $short_data, 'fastlz', 1.3, 4);
fetch_with_compression($m, 'hello06', $short_data, '',       1.3, 4);
fetch_with_compression($m, 'hello11', $data, 'zlib',         0.3, 4);
fetch_with_compression($m, 'hello12', $data, 'fastlz',       0.3, 4);
fetch_with_compression($m, 'hello13', $data, '',             0.3, 4);
fetch_with_compression($m, 'hello14', $short_data, 'zlib',   0.3, 4);
fetch_with_compression($m, 'hello15', $short_data, 'fastlz', 0.3, 4);
fetch_with_compression($m, 'hello16', $short_data, '',       0.3, 4);
fetch_with_compression($m, 'hello21', $data, 'zlib',         1.3, 2000);
fetch_with_compression($m, 'hello22', $data, 'fastlz',       1.3, 2000);
fetch_with_compression($m, 'hello23', $data, '',             1.3, 2000);
fetch_with_compression($m, 'hello24', $short_data, 'zlib',   1.3, 2000);
fetch_with_compression($m, 'hello25', $short_data, 'fastlz', 1.3, 2000);
fetch_with_compression($m, 'hello26', $short_data, '',       1.3, 2000);
fetch_with_compression($m, 'hello31', $data, 'zlib',         0.3, 2000);
fetch_with_compression($m, 'hello32', $data, 'fastlz',       0.3, 2000);
fetch_with_compression($m, 'hello33', $data, '',             0.3, 2000);
fetch_with_compression($m, 'hello34', $short_data, 'zlib',   0.3, 2000);
fetch_with_compression($m, 'hello35', $short_data, 'fastlz', 0.3, 2000);
fetch_with_compression($m, 'hello36', $short_data, '',       0.3, 2000);
?>
--EXPECT--
len=[4877] set=[zlib] factor=[1.3] threshold=[4]
bool(true)
len=[4877] set=[fastlz] factor=[1.3] threshold=[4]
bool(true)
len=[4877] set=[] factor=[1.3] threshold=[4]
bool(true)
len=[7] set=[zlib] factor=[1.3] threshold=[4]
bool(true)
len=[7] set=[fastlz] factor=[1.3] threshold=[4]
bool(true)
len=[7] set=[] factor=[1.3] threshold=[4]
bool(true)
len=[4877] set=[zlib] factor=[0.3] threshold=[4]
bool(true)
len=[4877] set=[fastlz] factor=[0.3] threshold=[4]
bool(true)
len=[4877] set=[] factor=[0.3] threshold=[4]
bool(true)
len=[7] set=[zlib] factor=[0.3] threshold=[4]
bool(true)
len=[7] set=[fastlz] factor=[0.3] threshold=[4]
bool(true)
len=[7] set=[] factor=[0.3] threshold=[4]
bool(true)
len=[4877] set=[zlib] factor=[1.3] threshold=[2000]
bool(true)
len=[4877] set=[fastlz] factor=[1.3] threshold=[2000]
bool(true)
len=[4877] set=[] factor=[1.3] threshold=[2000]
bool(true)
len=[7] set=[zlib] factor=[1.3] threshold=[2000]
bool(true)
len=[7] set=[fastlz] factor=[1.3] threshold=[2000]
bool(true)
len=[7] set=[] factor=[1.3] threshold=[2000]
bool(true)
len=[4877] set=[zlib] factor=[0.3] threshold=[2000]
bool(true)
len=[4877] set=[fastlz] factor=[0.3] threshold=[2000]
bool(true)
len=[4877] set=[] factor=[0.3] threshold=[2000]
bool(true)
len=[7] set=[zlib] factor=[0.3] threshold=[2000]
bool(true)
len=[7] set=[fastlz] factor=[0.3] threshold=[2000]
bool(true)
len=[7] set=[] factor=[0.3] threshold=[2000]
bool(true)
