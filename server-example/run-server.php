<?php

$server = new MemcachedServer();

$server->on (Memcached::ON_CONNECT,
             function ($remote_ip, $remote_port) {
                 echo "Incoming connection from {$remote_ip}:{$remote_port}" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_ADD,
             function ($client_id, $key, $value, $flags, $expiration, &$cas) {
                 echo "client_id=[$client_id]: Add key=[$key], value=[$value], flags=[$flags], expiration=[$expiration]" . PHP_EOL;
                 $cas = 15;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_APPEND,
             function ($client_id, $key, $value, $cas, &$result_cas) {
                 echo "client_id=[$client_id]: Append key=[$key], value=[$value], cas=[$cas]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_PREPEND,
             function ($client_id, $key, $value, $cas, &$result_cas) {
                 echo "client_id=[$client_id]: Prepend key=[$key], value=[$value], cas=[$cas]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_INCREMENT,
             function ($client_id, $key, $delta, $initial, $expiration, &$result, &$result_cas) {
                 echo "client_id=[$client_id]: Incrementing key=[$key], delta=[$delta], initial=[$initial], expiration=[$expiration]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_DECREMENT,
             function ($client_id, $key, $delta, $initial, $expiration, &$result, &$result_cas) {
                 echo "client_id=[$client_id]: Decrementing key=[$key], delta=[$delta], initial=[$initial], expiration=[$expiration]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_DELETE,
             function ($client_id, $key, $cas) {
                 echo "client_id=[$client_id]: Delete key=[$key], cas=[$cas]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_FLUSH,
             function ($client_id, $when) {
                 echo "client_id=[$client_id]: Flush when=[$when]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_GET,
             function ($client_id, $key, &$value, &$flags, &$cas) {
                 echo "client_id=[$client_id]: Get key=[$key]" . PHP_EOL;
                 $value = "Hello to you client!";
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_NOOP,
             function ($client_id) {
                 echo "client_id=[$client_id]: Noop" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_REPLACE,
             function ($client_id, $key, $value, $flags, $expiration, $cas, &$result_cas) {
                 echo "client_id=[$client_id]: Replace key=[$key], value=[$value], flags=[$flags], expiration=[$expiration], cas=[$cas]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });
             
$server->on (Memcached::ON_SET,
             function ($client_id, $key, $value, $flags, $expiration, $cas, &$result_cas) {
                 echo "client_id=[$client_id]: Set key=[$key], value=[$value], flags=[$flags], expiration=[$expiration], cas=[$cas]" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_STAT,
             function ($client_id, $key, &$value) {
                 echo "client_id=[$client_id]: Stat key=[$key]" . PHP_EOL;
                 $value = "Stat reply";
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->on (Memcached::ON_QUIT,
             function ($client_id) {
                 echo "client_id=[$client_id]: Client quit" . PHP_EOL;
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->run ("127.0.0.1:3434");