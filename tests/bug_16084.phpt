--TEST--
Memcached: Bug #16084 (Crash when addServers is called with an associative array)
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$servers = array ( 0 => array ( 'KEYHERE' => 'localhost', 11211, 3 ), );
$m = new memcached();
var_dump($m->addServers($servers));
var_dump($m->getServerList());
?>
--EXPECT--
bool(true)
array(1) {
  [0]=>
  array(2) {
    ["host"]=>
    string(9) "localhost"
    ["port"]=>
    int(11211)
  }
}
