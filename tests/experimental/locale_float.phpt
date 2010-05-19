--TEST--
Float should not consider locale
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) die("skip extension not loaded";
	if (!setlocale(LC_NUMERIC, "fi_FI", 'sv_SV', 'nl_NL')) {
		die("skip no suitable locale");
	}
--FILE--
<?php
$memcache = new Memcached();
$memcache->addServer('127.0.0.1', 11211);

setlocale(LC_NUMERIC,
	"fi_FI", 'sv_SV', 'nl_NL');
var_dump($memcache->set('test', 13882.1332451));
$n = $memcache->get('test');
setlocale(LC_NUMERIC, "C");
var_dump($n);

--EXPECT--
bool(true)
float(13882.1332451)
