--TEST--
Memcached phpser serializer with phpser.key rejects items it did not sign
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_PHPSER) print "skip phpser not enabled";
?>
--INI--
phpser.key=memcached-phpser-test-key-0123456789
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
include dirname(__FILE__) . '/phpser.inc';

$m = memc_get_instance(array(Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHPSER));
$php = memc_create_instance(MEMC_SERVER_HOST, MEMC_SERVER_PORT, array(Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHP));

$gadget = new PhpserGadget();
$gadget->cmd = 'rm -rf';
$gadget_phpser = phpser_serialize($gadget);
$gadget_igbinary = function_exists('igbinary_serialize') ? igbinary_serialize($gadget) : "\x00\x00\x00\x02\x17\x0cPhpserGadget";
unset($gadget);

$zlib = function ($data) {
	return pack('V', strlen($data)) . gzcompress($data);
};

$signed = phpser_serialize_keyed(array('k' => str_repeat('v', 50)), 'memcached');
$tampered = $signed;
$tampered[10] = chr(ord($tampered[10]) ^ 1);

$items = array(
	'ser_gadget'      => array(MEMC_TEST_IS_SERIALIZED, phpser_gadget_serialized()),
	'ser_gadget_zlib' => array(MEMC_TEST_IS_SERIALIZED | MEMC_TEST_COMPRESSED_ZLIB, $zlib(phpser_gadget_serialized())),
	'igbinary'        => array(MEMC_TEST_IS_IGBINARY, $gadget_igbinary),
	'string'          => array(MEMC_TEST_IS_STRING, 'attacker text'),
	'bool'            => array(MEMC_TEST_IS_BOOL, '1'),
	'json'            => array(MEMC_TEST_IS_JSON, '{"a":1}'),
	'msgpack'         => array(MEMC_TEST_IS_MSGPACK, "\x81\xa1a\x01"),
	'unknown_type'    => array(9, 'x'),
	'phpser_unsigned' => array(MEMC_TEST_IS_PHPSER, $gadget_phpser),
	'phpser_unsigned_zlib' => array(MEMC_TEST_IS_PHPSER | MEMC_TEST_COMPRESSED_ZLIB, $zlib($gadget_phpser)),
	'phpser_wrongkey' => array(MEMC_TEST_IS_PHPSER, phpser_forge_signed(array('evil'), str_repeat('z', 32))),
	'phpser_ctx'      => array(MEMC_TEST_IS_PHPSER, phpser_forge_signed(array('evil'), PHPSER_TEST_KEY, 'redis')),
	'phpser_tampered' => array(MEMC_TEST_IS_PHPSER, $tampered),
	'phpser_truncated' => array(MEMC_TEST_IS_PHPSER, substr($signed, 0, -1)),
	'phpser_empty'    => array(MEMC_TEST_IS_PHPSER, ''),
	'good'            => array(MEMC_TEST_IS_PHPSER, phpser_forge_signed(array('ok'), PHPSER_TEST_KEY)),
	'good_zlib'       => array(MEMC_TEST_IS_PHPSER | MEMC_TEST_COMPRESSED_ZLIB, $zlib($signed)),
	'native_long'     => array(MEMC_TEST_IS_LONG, '7'),
	'native_double'   => array(MEMC_TEST_IS_DOUBLE, '2.5'),
);

foreach ($items as $key => $item) {
	phpser_raw_set($key, $item[0], $item[1]);
}

/* Other serializers write into the same pool */
$php->set('php_client_array', array('from' => 'php serializer'));
$php->set('php_client_string', 'plain string');
$php->set('php_client_bool', true);
foreach (array('php_client_array', 'php_client_string', 'php_client_bool') as $key) {
	$items[$key] = null;
}

PhpserGadget::$events = array();

$warnings = array();
set_error_handler(function ($no, $str) use (&$warnings) {
	$warnings[] = preg_replace('/^Memcached::\w+\(\): /', '', $str);
	return true;
});

foreach (array_keys($items) as $key) {
	$warnings = array();
	$value = $m->get($key);
	printf("%-20s %s rc=%d warnings=%d%s\n", $key,
		is_array($value) ? 'array(' . count($value) . ')' : var_export($value, true),
		$m->getResultCode(), count($warnings), $warnings ? ' ' . $warnings[0] : '');
}

restore_error_handler();
echo "gadget events: ", json_encode(PhpserGadget::$events), "\n";
var_dump(Memcached::RES_SOME_ERRORS);
?>
--EXPECT--
ser_gadget           false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
ser_gadget_zlib      false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
igbinary             false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
string               false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
bool                 false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
json                 false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
msgpack              false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
unknown_type         false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
phpser_unsigned      false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
phpser_unsigned_zlib false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
phpser_wrongkey      false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
phpser_ctx           false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
phpser_tampered      false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
phpser_truncated     false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
phpser_empty         false rc=19 warnings=1 phpser: memcached value failed HMAC verification, not decoded
good                 array(1) rc=0 warnings=0
good_zlib            array(1) rc=0 warnings=0
native_long          7 rc=0 warnings=0
native_double        2.5 rc=0 warnings=0
php_client_array     false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
php_client_string    false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
php_client_bool      false rc=19 warnings=1 could not unserialize value: phpser.key is set and the value is not signed phpser
gadget events: []
int(19)
