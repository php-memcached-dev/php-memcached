--TEST--
Memcached::phpinfo()
--SKIPIF--
<?php 
include dirname(dirname(__FILE__)) . "/skipif.inc";
if (!Memcached::HAVE_SESSION) print "skip";
?>
--FILE--
<?php
ob_start();
phpinfo(INFO_MODULES);
$output = ob_get_clean();

$array = explode("\n", $output);
$array = preg_grep('/^memcached/', $array);

echo implode("\n", $array);

--EXPECTF--
memcached
memcached support => enabled
%A
