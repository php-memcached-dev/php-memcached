--TEST--
Session lock
--SKIPIF--
<?php 
include dirname(__FILE__) . "/skipif.inc"; 
if (!Memcached::HAVE_SESSION) print "skip";

if (PHP_VERSION_ID < 70100) print "skip";
?>
--INI--
memcached.sess_locking       = true
memcached.sess_lock_wait_min = 500
memcached.sess_lock_wait_max = 1000
memcached.sess_lock_retries  = 3
memcached.sess_prefix        = "memc.test."

# Turn off binary protocol while the test matrix has older versions of
# libmemcached for which the extension warns of a broken touch command.
memcached.sess_binary_protocol = Off

session.save_handler = memcached

--FILE--
<?php

include dirname (__FILE__) . '/config.inc';

$m = new Memcached();
$m->addServer(MEMC_SERVER_HOST, MEMC_SERVER_PORT);

ob_start();
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

session_start();
$session_id = session_id();

$_SESSION["test"] = "hello";
session_write_close();

session_start();
var_dump ($m->get ('memc.test.' . session_id()));
var_dump ($m->get ('memc.test.lock.' . session_id()));
session_write_close();
var_dump ($m->get ('memc.test.lock.' . session_id()));

// Test lock min / max
$m->set ('memc.test.lock.' . $session_id, '1');

$time_start = microtime(true);
session_start();
$time = microtime(true) - $time_start;

if (round ($time, 1) != 2.5) {
	echo "Waited longer than expected: $time" . PHP_EOL;
}
echo "OK";

--EXPECTF--
string(17) "test|s:5:"hello";"
string(1) "1"
bool(false)

Warning: session_start(): Unable to clear session lock record in %s on line %d

Warning: session_start(): Failed to read session data: memcached (path: 127.0.0.1:11211) in %s on line %d
OK
