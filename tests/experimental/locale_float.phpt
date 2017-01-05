--TEST--
Float should not consider locale
--SKIPIF--
<?php
include dirname(dirname(__FILE__)) . "/skipif.inc";
if (!setlocale(LC_NUMERIC, "fi_FI", 'sv_SV', 'nl_NL')) {
	die("skip no suitable locale");
}
--FILE--
<?php

include dirname(dirname(__FILE__)) . '/config.inc';
$memcache = memc_get_instance ();

setlocale(LC_NUMERIC,
	"fi_FI", 'sv_SV', 'nl_NL');
var_dump($memcache->set('test', 13882.1332451));
$n = $memcache->get('test');
setlocale(LC_NUMERIC, "C");
var_dump($n);

--EXPECT--
bool(true)
float(13882.1332451)
