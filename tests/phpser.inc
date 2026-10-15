<?php

define('PHPSER_TEST_KEY', 'memcached-phpser-test-key-0123456789');

define('MEMC_TEST_IS_STRING',     0);
define('MEMC_TEST_IS_LONG',       1);
define('MEMC_TEST_IS_DOUBLE',     2);
define('MEMC_TEST_IS_BOOL',       3);
define('MEMC_TEST_IS_SERIALIZED', 4);
define('MEMC_TEST_IS_IGBINARY',   5);
define('MEMC_TEST_IS_JSON',       6);
define('MEMC_TEST_IS_MSGPACK',    7);
define('MEMC_TEST_IS_PHPSER',     8);
define('MEMC_TEST_COMPRESSED_ZLIB', (1 << 4) | (2 << 4));

class PhpserGadget {
	public static $events = [];
	public $cmd = 'none';

	public function __wakeup() {
		self::$events[] = "wakeup";
	}

	public function __unserialize(array $data) {
		self::$events[] = "unserialize";
		$this->cmd = $data['cmd'] ?? 'none';
	}

	public function __destruct() {
		self::$events[] = "destruct";
	}
}

/* Writes an item with caller-chosen flags, as anyone with write access to
 * the memcached server can. */
function phpser_raw_set($key, $flags, $data)
{
	$fp = fsockopen(MEMC_SERVER_HOST, MEMC_SERVER_PORT, $errno, $errstr, 2);
	fwrite($fp, "set $key $flags 0 " . strlen($data) . "\r\n" . $data . "\r\n");
	$reply = trim(fgets($fp));
	fclose($fp);
	if ($reply !== "STORED") {
		echo "raw set of $key failed: $reply\n";
	}
}

/* Returns [flags, data] for a key, read without any client-side decoding. */
function phpser_raw_get($key)
{
	$fp = fsockopen(MEMC_SERVER_HOST, MEMC_SERVER_PORT, $errno, $errstr, 2);
	fwrite($fp, "get $key\r\n");
	$header = trim(fgets($fp));
	if ($header === "END") {
		fclose($fp);
		return null;
	}
	list(, , $flags, $len) = explode(' ', $header);
	$data = '';
	while (strlen($data) < $len) {
		$data .= fread($fp, $len - strlen($data));
	}
	fclose($fp);
	return [(int) $flags, $data];
}

function phpser_raw_type($key)
{
	$item = phpser_raw_get($key);
	return $item === null ? null : ($item[0] & 0xf);
}

/* Signs a phpser frame the way phpser does for phpser.key = $key. */
function phpser_forge_signed($value, $key, $context = 'memcached')
{
	$frame = phpser_serialize($value);
	$subkey = hash_hmac('sha256', "phpser:" . $context . ":v1\0", $key, true);
	return $frame . hash_hmac('sha256', $frame, $subkey, true);
}

function phpser_gadget_serialized()
{
	return 'O:12:"PhpserGadget":1:{s:3:"cmd";s:6:"rm -rf";}';
}
