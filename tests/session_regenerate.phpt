--TEST--
Session regenerate
--SKIPIF--
<?php 
include dirname(__FILE__) . "/skipif.inc"; 
if (!Memcached::HAVE_SESSION) print "skip";
?>
--INI--
session.save_handler = memcached
--FILE--
<?php
ob_start();

include dirname (__FILE__) . '/config.inc';
ini_set ('session.save_path', MEMC_SERVER_HOST . ':' . MEMC_SERVER_PORT);

var_dump(session_start());
var_dump(session_regenerate_id());
var_dump(session_regenerate_id(true));
echo "OK";

--EXPECT--
bool(true)
bool(true)
bool(true)
OK
