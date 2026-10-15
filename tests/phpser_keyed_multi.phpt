--TEST--
Memcached phpser serializer with phpser.key gates getMulti, getDelayed, fetch, byKey, and callbacks
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

$m->set('good1', array('one'));
$m->set('good2', 'two');
$m->set('num', 3);
phpser_raw_set('evil_ser', MEMC_TEST_IS_SERIALIZED, phpser_gadget_serialized());
phpser_raw_set('evil_str', MEMC_TEST_IS_STRING, 'forged');
phpser_raw_set('evil_unsigned', MEMC_TEST_IS_PHPSER, phpser_serialize(array('forged')));
PhpserGadget::$events = array();

$keys = array('good1', 'evil_ser', 'good2', 'evil_str', 'num', 'evil_unsigned');

/* One warning per rejected item, and the rejected values must not reach
 * any caller, whatever the entry point. */
$warnings = 0;
set_error_handler(function () use (&$warnings) { $warnings++; return true; });

function show($label, $value, $m, &$warnings) {
	echo $label, ": ", json_encode($value), " rc=", $m->getResultCode(), " warnings=", $warnings, "\n";
	$warnings = 0;
}

show('getMulti', $m->getMulti($keys), $m, $warnings);
show('getMultiByKey', $m->getMultiByKey('srv', $keys), $m, $warnings);

$ext = $m->getMulti($keys, Memcached::GET_EXTENDED);
show('getMulti extended keys', array_keys($ext), $m, $warnings);

show('getByKey evil', $m->getByKey('srv', 'evil_ser'), $m, $warnings);
show('getByKey good', $m->getByKey('srv', 'good1'), $m, $warnings);
show('get extended evil', $m->get('evil_str', null, Memcached::GET_EXTENDED), $m, $warnings);

/* A decode failure makes fetch() return false and drops the next item,
 * and makes fetchAll() return false; that is the existing contract for any
 * value that does not unserialize. */
$m->getDelayed($keys);
$seq = array();
for ($i = 0; $i < 5; $i++) {
	$r = $m->fetch();
	$seq[] = ($r ? $r['key'] . '=' . json_encode($r['value']) : 'false') . ' rc=' . $m->getResultCode();
}
show('getDelayed+fetch', $seq, $m, $warnings);

$m->getDelayed($keys, true);
show('getDelayed(cas)+fetchAll', $m->fetchAll(), $m, $warnings);

$cb = array();
$m->getDelayedByKey('srv', $keys, false, function ($memc, $r) use (&$cb) {
	$cb[$r['key']] = $r['value'];
});
ksort($cb);
show('getDelayedByKey callback', $cb, $m, $warnings);

$called = 0;
$v = $m->get('evil_ser', function ($memc, $key, &$value) use (&$called) {
	$called++;
	$value = 'from callback';
	return true;
});
show('read-through on rejected item', array($v, $called), $m, $warnings);

$v = $m->get('missing', function ($memc, $key, &$value) {
	$value = 'from callback';
	return true;
});
show('read-through on miss', $v, $m, $warnings);
show('read-through value stored signed', array($m->get('missing'), phpser_raw_type('missing')), $m, $warnings);

restore_error_handler();
echo "gadget events: ", json_encode(PhpserGadget::$events), "\n";
?>
--EXPECT--
getMulti: {"good1":["one"],"good2":"two","num":3} rc=19 warnings=3
getMultiByKey: {"good1":["one"],"good2":"two","num":3} rc=19 warnings=3
getMulti extended keys: ["good1","good2","num"] rc=19 warnings=3
getByKey evil: false rc=19 warnings=1
getByKey good: ["one"] rc=0 warnings=0
get extended evil: false rc=19 warnings=1
getDelayed+fetch: ["good1=[\"one\"] rc=0","false rc=19","false rc=19","false rc=19","false rc=16"] rc=16 warnings=3
getDelayed(cas)+fetchAll: false rc=19 warnings=3
getDelayedByKey callback: {"good1":["one"],"good2":"two","num":3} rc=19 warnings=3
read-through on rejected item: [false,0] rc=19 warnings=1
read-through on miss: "from callback" rc=0 warnings=0
read-through value stored signed: ["from callback",8] rc=0 warnings=0
gadget events: []
