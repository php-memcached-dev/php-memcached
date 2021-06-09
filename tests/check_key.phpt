--TEST--
Memcached::checkKey()
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (
				Memcached::OPT_BINARY_PROTOCOL => false,
				Memcached::OPT_VERIFY_KEY => true
		));

$keys = [
	'foo',
	'foo bar',
	str_repeat('a',65),
	str_repeat('b',250),
	str_repeat('c',251),
	'Montréal',
	'København',
	'Düsseldorf',
	'Kraków',
	'İstanbul',
	'ﺎﺨﺘﺑﺍﺭ PHP',
	'測試',
	'Тестирование',
	'پی ایچ پی کی جانچ ہو رہی ہے',
	'Testataan PHP: tä',
	'Að prófa PHP',
	'د پی ایچ پی ازمول',
	'Pruvà PHP'
];
foreach($keys as $key) {
	echo "Checking \"$key\"" . PHP_EOL;
	echo "MEMC_CHECK_KEY: ";
	var_dump($m->checkKey($key));
	echo "libmemcached: ";
	var_dump($m->set($key, "this is a test"));
    var_dump($m->getResultMessage());
	echo "\n";
}
--EXPECT--
Checking "foo"
MEMC_CHECK_KEY: bool(true)
libmemcached: bool(true)
string(7) "SUCCESS"

Checking "foo bar"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
MEMC_CHECK_KEY: bool(true)
libmemcached: bool(true)
string(7) "SUCCESS"

Checking "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
MEMC_CHECK_KEY: bool(true)
libmemcached: bool(true)
string(7) "SUCCESS"

Checking "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Montréal"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "København"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Düsseldorf"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Kraków"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "İstanbul"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "ﺎﺨﺘﺑﺍﺭ PHP"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "測試"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Тестирование"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "پی ایچ پی کی جانچ ہو رہی ہے"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Testataan PHP: tä"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Að prófa PHP"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "د پی ایچ پی ازمول"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

Checking "Pruvà PHP"
MEMC_CHECK_KEY: bool(false)
libmemcached: bool(false)
string(46) "A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE"

