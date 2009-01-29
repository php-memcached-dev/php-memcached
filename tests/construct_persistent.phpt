--TEST--
persistent memcached connection
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$m1 = new Memcached('id1');
$m1->setOption(Memcached::OPT_PREFIX_KEY, 'php');
var_dump($m1->getOption(Memcached::OPT_PREFIX_KEY));

$m2 = new Memcached('id1');
var_dump($m1->getOption(Memcached::OPT_PREFIX_KEY));

$m3 = new Memcached();
var_dump($m3->getOption(Memcached::OPT_PREFIX_KEY));
?>
--EXPECT--
string(3) "php"
string(3) "php"
string(0) ""
