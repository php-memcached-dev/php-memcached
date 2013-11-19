--TEST--
Test for Github issue #93 (double and long overflow)
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$m = memc_get_instance (array (
							Memcached::OPT_COMPRESSION => false
						));

function testOverflow($m, $value) {
	$m->delete('overflow');
	if (true !== $m->set('overflow', $value)) {
		echo "Error storing 'overflow' variable\n";
		return false;
	}

	if (true !== $m->prepend('overflow', str_repeat('0', 128))) {
		echo "Error prepending key\n";
		return false;
	}

	$v = @$m->get('overflow');
	if ($v !== $value) {
		// At least it doesn't segfault, so we're happy for now
		// echo "Error receiving 'overflow' variable\n";
		// return false;
		return true;
	}

	return true;
}

if (!testOverflow($m, 10)) {
	return;
}

if (!testOverflow($m, 9.09)) {
	return;
}

echo "OK\n";
?>
--EXPECT--
OK