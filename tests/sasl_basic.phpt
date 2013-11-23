--TEST--
Test SASL authentication
--SKIPIF--
<?php
include dirname (__FILE__) . '/config.inc';
if (!extension_loaded("memcached")) die ("skip");
if (!Memcached::HAVE_SASL) die ("skip no SASL support enabled");
if (!defined ('MEMC_SASL_USER') || empty (MEMC_SASL_USER)) die ("skip MEMC_SASL_USER is not set");
?>
--INI--
memcached.use_sasl = on
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_sasl_instance (array (
								Memcached::OPT_BINARY_PROTOCOL => true
							));

var_dump ($m->setSaslAuthData (MEMC_SASL_USER, MEMC_SASL_PASS));

$key = uniqid ('sasl_test_');
var_dump ($m->set ($key, 'set using sasl'));
var_dump ($m->get ($key));


echo "OK" . PHP_EOL;
?>
--EXPECT--
bool(true)
bool(true)
string(14) "set using sasl"
OK
