--TEST--
Memcached::getMulti() bad server
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
var_dump($m->getMulti(array('foo', 'bar')));
echo $m->getResultMessage(), "\n";

$m->addServer('localhost', 37712, 1);

var_dump($m->getMulti(array('foo', 'bar')));
switch ($m->getResultCode()) {
	case Memcached::RES_ERRNO:
	case Memcached::RES_SOME_ERRORS:
	case Memcached::RES_FAILURE:
		break;
	default:
		echo $m->getResultCode(), ": ";
		echo $m->getResultMessage(), "\n";
}

--EXPECTF--
array(0) {
}
NO SERVERS DEFINED
array(0) {
}
%d: %s
