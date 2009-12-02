--TEST--
Serialize igbinary
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) print "skip";
	if (!extension_loaded('igbinary')) echo "skip no igbinary loaded";
	if (!Memcached::HAVE_IGBINARY) echo "skip igbinary support not enabled";

?>
--REDIRECTTEST--

$path = implode(DIRECTORY_SEPARATOR, array('tests', 'experimental', 'serializer'));

return array(
	'TESTS' => $path,
	'ENV' => array('TEST_MEMC_SERIALIZER' => 'Memcached::SERIALIZER_IGBINARY'),
);
