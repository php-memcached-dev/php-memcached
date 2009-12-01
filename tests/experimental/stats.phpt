--TEST--
Memcached::getStats()
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
$m = new Memcached();
var_dump($m->getStats());

$m->addServer('localhost', 11211, 1);

$stats = $m->getStats();
var_dump($stats);

--EXPECTF--
array(0) {
}
array(%d) {
  ["%s:%d"]=>
  array(%d) {
    ["pid"]=>
    int(%d)
    ["uptime"]=>
    int(%d)
    ["threads"]=>
    int(%d)
    ["time"]=>
    int(%d)
    ["pointer_size"]=>
    int(%d)
    ["rusage_user_seconds"]=>
    int(%d)
    ["rusage_user_microseconds"]=>
    int(%d)
    ["rusage_system_seconds"]=>
    int(%d)
    ["rusage_system_microseconds"]=>
    int(%d)
    ["curr_items"]=>
    int(%d)
    ["total_items"]=>
    int(%d)
    ["limit_maxbytes"]=>
    int(%d)
    ["curr_connections"]=>
    int(%d)
    ["total_connections"]=>
    int(%d)
    ["connection_structures"]=>
    int(%d)
    ["bytes"]=>
    int(%d)
    ["cmd_get"]=>
    int(%d)
    ["cmd_set"]=>
    int(%d)
    ["get_hits"]=>
    int(%d)
    ["get_misses"]=>
    int(%d)
    ["evictions"]=>
    int(%d)
    ["bytes_read"]=>
    int(%d)
    ["bytes_written"]=>
    int(%d)
    ["version"]=>
    string(%d) "%s"
  }
}
