--TEST--
Serialize JSON
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) print "skip";
	if (!extension_loaded('json')) echo "skip no JSON loaded";
	if (!Memcached::HAVE_JSON) echo "skip JSON support not enabled";
?>
--REDIRECTTEST--

$path = implode(DIRECTORY_SEPARATOR, array('tests', 'experimental', 'serializer'));

return array(
	'TESTS' => $path,
	'ENV' => array('TEST_MEMC_SERIALIZER' => 'Memcached::SERIALIZER_JSON'),
);
