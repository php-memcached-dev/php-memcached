--TEST--
Serialize JSON
--SKIPIF--
<?php
	if (!extension_loaded("memcached")) print "skip";
	if (!extension_loaded('json')) echo "skip no JSON loaded";
	if (!Memcached::HAVE_JSON) echo "skip JSON support not enabled";
?>
--REDIRECTTEST--

return array(
	'TESTS' => sys_get_temp_dir(),
	'ENV' => array('TEST_MEMC_SERIALIZER' => 'Memcached::SERIALIZER_JSON'),
);
