--TEST--
Set default serializer
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php

function comp_serializer(Memcached $old_m, $serializer, $m_serializer) {
	$old_s = $old_m->getOption(Memcached::OPT_SERIALIZER);
	$ok = ini_set('memcached.serializer', $serializer);
	if ($ok !== false) {
		$ok = ini_get('memcached.serializer') == $serializer;
	}

	if ($ok) {
		$m = new Memcached();
		$s = $m->getOption(Memcached::OPT_SERIALIZER);
		if ($s != $m_serializer) {
			echo "Serializer is $s, should be $m_serializer ($serializer)\n";
		}
	}

	if ($old_m->getOption(Memcached::OPT_SERIALIZER) != $old_s) {
		echo "Should not change instantiated objects.\n";
	}
}


$m = new Memcached();
$serializer = ini_get('memcached.serializer');
$s = $m->getOption(Memcached::OPT_SERIALIZER);
if ($serializer == "igbinary" && $s == Memcached::SERIALIZER_IGBINARY
	|| $serializer == "php" && $s == Memcached::SERIALIZER_PHP) {
	echo "OK: $serializer\n";
} else {
	echo "Differing serializer: $serializer vs. $s\n";
}

comp_serializer($m, 'json', Memcached::SERIALIZER_JSON);
comp_serializer($m, 'igbinary', Memcached::SERIALIZER_IGBINARY);
comp_serializer($m, 'json_array', Memcached::SERIALIZER_JSON_ARRAY);
comp_serializer($m, 'php', Memcached::SERIALIZER_PHP);

--EXPECTF--
OK: %rigbinary|php%r
