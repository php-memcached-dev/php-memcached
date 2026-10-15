<?php

function memc_raw_command($command)
{
	$fp = fsockopen(MEMC_SERVER_HOST, MEMC_SERVER_PORT, $errno, $errstr, 2);
	fwrite($fp, $command);
	$reply = '';
	while (!feof($fp)) {
		$line = fgets($fp);
		$reply .= $line;
		if ($line === "END\r\n" || $line === "STORED\r\n" || strncmp($line, "ERROR", 5) === 0) {
			break;
		}
	}
	fclose($fp);
	return $reply;
}

/* Returns [flags, data] as stored, without client-side decoding. */
function memc_raw_get($key)
{
	$reply = memc_raw_command("get $key\r\n");
	$header_end = strpos($reply, "\r\n");
	list(, , $flags, $len) = explode(' ', substr($reply, 0, $header_end));
	return array((int) $flags, substr($reply, $header_end + 2, (int) $len));
}

function memc_raw_set($key, $flags, $data)
{
	return trim(memc_raw_command("set $key $flags 0 " . strlen($data) . "\r\n" . $data . "\r\n"));
}

/* Stores $value compressed with $type, then rewrites the 4-byte uncompressed
 * length header that precedes the compressed data, and reads it back. */
function memc_compressed_length_test($m, $name, $type, $value)
{
	$m->setOption(Memcached::OPT_COMPRESSION, true);
	$m->setOption(Memcached::OPT_COMPRESSION_TYPE, $type);
	$m->set('length_test', $value);
	list($flags, $data) = memc_raw_get('length_test');
	$real = unpack('V', substr($data, 0, 4))[1];
	$body = substr($data, 4);

	$cases = array(
		'unchanged' => $real,
		'larger by 4096' => $real + 4096,
		'smaller by 1' => $real - 1,
		'1 GiB' => 1 << 30,
		'4 GiB - 1' => 0xFFFFFFFF,
	);
	foreach ($cases as $label => $length) {
		memc_raw_set('length_test', $flags, pack('V', $length) . $body);
		error_clear_last();
		$result = @$m->get('length_test');
		$error = error_get_last();
		printf("%s %s: %s%s\n", $name, $label,
			$result === $value ? 'original value' : var_export($result, true),
			$error ? ' (' . preg_replace('/^Memcached::get\(\): /', '', $error['message']) . ')' : '');
	}
}
