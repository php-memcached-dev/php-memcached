--TEST--
Memcached::set()/delete() UDP
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
$m->addServer('127.0.0.1', 11211, 1);

$m_udp = new Memcached();
$m_udp->setOption(Memcached::OPT_USE_UDP, true);
$m_udp->addServer('127.0.0.1', 11211, 1);



error_reporting(0);

echo "\n";
echo "Delete not found\n";
$m->delete('foo');
var_dump($m_udp->delete('foo'));
echo $m_udp->getResultMessage(), "\n";

echo "\n";
echo "Set\n";
var_dump($m_udp->set('foo', "asdf", 10));
sleep (1);

echo $m_udp->getResultMessage(), "\n";
var_dump($m->get('foo'));

echo "\n";
echo "Delete found\n";
var_dump($m_udp->delete('foo'));
sleep (1);

echo $m_udp->getResultMessage(), "\n";
$m->get('foo');
echo $m->getResultMessage(), "\n";


--EXPECT--
Delete not found
bool(true)
SUCCESS

Set
bool(true)
SUCCESS
string(4) "asdf"

Delete found
bool(true)
SUCCESS
NOT FOUND
