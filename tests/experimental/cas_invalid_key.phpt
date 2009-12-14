--TEST--
Memcached::cas() with strange key
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('localhost', 11211, 1);

error_reporting(0);
var_dump($m->cas(0, '', true, 10));
echo $m->getResultMessage(), "\n";

var_dump($m->cas(0, ' дц jas kjjhask d ', true, 10));
echo $m->getResultMessage(), "\n";

--EXPECTF--
bool(false)
A BAD KEY WAS PROVIDED/CHARACTERS OUT OF RANGE
bool(false)
%rCLIENT ERROR|NOT FOUND%r
