--TEST--
Session bad configurations, persistent
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
session.save_handler = "memcached"
session.save_path = "PERSISTENT=1 hello:11211,world:11211"

--FILE--
<?php
ob_start();

session_start();
session_write_close();

// In PHP < 7.2 this is a Fatal Error so the OK is not printed
// echo "OK";

--EXPECTF--
Warning: session_start(): failed to parse session.save_path: PERSISTENT is replaced by memcached.sess_persistent = On in %s on line %d

%s: session_start(): Failed to initialize storage module: memcached (path: PERSISTENT=1 %s) in %s on line %d
