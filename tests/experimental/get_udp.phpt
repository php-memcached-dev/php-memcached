--TEST--
Memcached::set()/delete() UDP
--SKIPIF--
<?php include dirname(dirname(__FILE__)) . "/skipif.inc";?>
--FILE--
<?php
include dirname(dirname(__FILE__)) . "/config.inc";
$m_udp = new Memcached();
$m_udp->setOption(Memcached::OPT_USE_UDP, true);
$m_udp->addServer("127.0.0.1", 11211, 1);

echo "\nGet incompatible\n";
var_dump($m_udp->get("foo", null, Memcached::GET_EXTENDED));
echo $m_udp->getResultMessage(), "\n";

echo "\nDelete not found\n";
$m_udp->delete("foo");
var_dump($m_udp->delete("foo"));
echo $m_udp->getResultMessage(), "\n";

echo "\nSet\n";
var_dump($m_udp->set("foo", "asdf", 10));
echo $m_udp->getResultMessage(), "\n";

echo "\nGet found\n";
var_dump($m_udp->get("foo"));
echo $m_udp->getResultMessage(), "\n";

echo "\nDelete found\n";
var_dump($m_udp->delete("foo"));
echo $m_udp->getResultMessage(), "\n";

echo "\nGet not found\n";
$m_udp->get("foo");
echo $m_udp->getResultMessage(), "\n";

echo "\nOK\n";

--EXPECTF--
Get incompatible

Warning: Memcached::get(): Cannot use GET_EXTENDED in UDP mode in %s on line %d

Delete not found
bool(true)
SUCCESS

Set
bool(true)
SUCCESS

Get found
string(4) "asdf"
SUCCESS

Delete found
bool(true)
SUCCESS

Get not found
NOT FOUND

OK
