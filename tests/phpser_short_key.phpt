--TEST--
Memcached phpser serializer rejects values when phpser.key is too short
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_PHPSER) print "skip phpser not enabled";
?>
--INI--
phpser.key=too-short
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';
include dirname(__FILE__) . '/phpser.inc';

$m = memc_get_instance(array(Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHPSER));

$warnings = array();
set_error_handler(function ($no, $str) use (&$warnings) {
	$warnings[] = preg_replace('/^Memcached::\w+\(\): /', '', $str);
	return true;
});

function show($label, $value, $m, &$warnings) {
	echo $label, ": ", var_export($value, true), " rc=", $m->getResultCode(), " ", json_encode($warnings), "\n";
	$warnings = array();
}

show('set string', $m->set('s', 'value'), $m, $warnings);
show('set array', $m->set('a', array(1)), $m, $warnings);
show('set bool', $m->set('b', true), $m, $warnings);
show('set long', $m->set('l', 5), $m, $warnings);
show('get long', $m->get('l'), $m, $warnings);
show('increment', $m->increment('l'), $m, $warnings);

phpser_raw_set('unsigned', MEMC_TEST_IS_PHPSER, phpser_serialize(array('forged')));
phpser_raw_set('signed_short', MEMC_TEST_IS_PHPSER, phpser_forge_signed(array('forged'), 'too-short'));
phpser_raw_set('ser', MEMC_TEST_IS_SERIALIZED, phpser_gadget_serialized());
phpser_raw_set('str', MEMC_TEST_IS_STRING, 'forged');
PhpserGadget::$events = array();

show('get unsigned', $m->get('unsigned'), $m, $warnings);
show('get signed with short key', $m->get('signed_short'), $m, $warnings);
show('get serialized', $m->get('ser'), $m, $warnings);
show('get string', $m->get('str'), $m, $warnings);

restore_error_handler();
echo "gadget events: ", json_encode(PhpserGadget::$events), "\n";
var_dump(Memcached::RES_PAYLOAD_FAILURE);
?>
--EXPECT--
set string: false rc=-1001 ["phpser.key must be at least 32 bytes; keyed value rejected"]
set array: false rc=-1001 ["phpser.key must be at least 32 bytes; keyed value rejected"]
set bool: false rc=-1001 ["phpser.key must be at least 32 bytes; keyed value rejected"]
set long: true rc=0 []
get long: 5 rc=0 []
increment: 6 rc=0 []
get unsigned: false rc=19 ["phpser.key must be at least 32 bytes; keyed value rejected"]
get signed with short key: false rc=19 ["phpser.key must be at least 32 bytes; keyed value rejected"]
get serialized: false rc=19 ["could not unserialize value: phpser.key is set and the value is not signed phpser"]
get string: false rc=19 ["could not unserialize value: phpser.key is set and the value is not signed phpser"]
gadget events: []
int(-1001)
