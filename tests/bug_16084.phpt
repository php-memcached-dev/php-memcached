--TEST--
Memcached: Bug #16084 (Crash when addServers is called with an associative array)
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$servers = array ( 0 => array ( 'KEYHERE' => 'localhost', 11211, 3 ), );
$m = new memcached();
$m->addServers($servers);
var_dump($m->getServerList());
?>
--EXPECT--
array(1) {
  [0]=>
  array(3) {
    ["host"]=>
    string(9) "localhost"
    ["port"]=>
    int(11211)
    ["weight"]=>
    int(3)
  }
}
