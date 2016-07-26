--TEST--
Memcached: Bug #16084 (Crash when addServers is called with an associative array)
--SKIPIF--
<?php include "skipif.inc";?>
--FILE--
<?php 
$servers = array ( 0 => array ( 'KEYHERE' => 'localhost', 11211, 3 ), );
$m = new memcached();
var_dump($m->addServers($servers));
$list = $m->getServerList();

var_dump ($list[0]["host"], $list[0]["port"]);
echo "OK";

?>
--EXPECT--
bool(true)
string(9) "localhost"
int(11211)
OK
