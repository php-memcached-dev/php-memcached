<?php

define ("MEMC_SERVER_HOST", "127.0.0.1");
define ("MEMC_SERVER_PORT", 11211);

function memc_get_instance (array $opts = array (), $persistent_id = null)
{
	$memcached = new Memcached($persistent_id);
	if ($memcached->setOptions ($opts) == false)
		echo "Failed to set options" . PHP_EOL;

	$memcached->addServer(MEMC_SERVER_HOST, MEMC_SERVER_PORT);
	$memcached->flush ();
	return $memcached;
}

function memc_run_test ($test_function, $options = array ())
{
	foreach ($options as $option_set) {
		$memc = memc_get_instance ($option_set ['options']);
		$test_function ($memc, $option_set);
	}
	echo "TEST DONE" . PHP_EOL;
}

function memc_create_combinations ($name, $serializer, $ignore_object_type = false)
{
	return array (
			array (
				'title' => "$name serializer, ascii protocol",
				'options' => array (
					Memcached::OPT_SERIALIZER => $serializer
				),
				'ignore_object_type' => $ignore_object_type
			),
			array (
				'title' => "$name serializer, binary protocol",
				'options' => array (
					Memcached::OPT_BINARY_PROTOCOL => true,
					Memcached::OPT_SERIALIZER => $serializer
				),
				'ignore_object_type' => $ignore_object_type
			),
	);
}

function memc_get_serializer_options ($skipmsgpack=false)
{
	$options = memc_create_combinations ('PHP', Memcached::SERIALIZER_PHP);

	if (Memcached::HAVE_IGBINARY) {
		$options = array_merge ($options, memc_create_combinations ('igbinary', Memcached::SERIALIZER_IGBINARY));
	}

	if (Memcached::HAVE_JSON) {
		$options = array_merge ($options, memc_create_combinations ('JSON', Memcached::SERIALIZER_JSON, true));
	}

	if (Memcached::HAVE_MSGPACK && !$skipmsgpack) {
		$options = array_merge ($options, memc_create_combinations ('msgpack', Memcached::SERIALIZER_MSGPACK));
	}
	return $options;
}
