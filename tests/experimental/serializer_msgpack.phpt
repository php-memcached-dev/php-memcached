--TEST--
Serialize msgpack
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) print "skip";
	if (!extension_loaded('msgpack')) echo "skip no msgpack loaded";
	if (!Memcached::HAVE_MSGPACK) echo "skip msgpack support not enabled";
?>
--REDIRECTTEST--

$path = implode(DIRECTORY_SEPARATOR, array('tests', 'experimental', 'serializer'));

return array(
	'TESTS' => $path,
	'ENV' => array('TEST_MEMC_SERIALIZER' => 'Memcached::SERIALIZER_MSGPACK'),
);
