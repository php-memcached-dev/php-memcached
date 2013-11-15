<?php

$server = new MemcachedServer();

class Storage {
    private $values = array ();
    
    public function set ($key, $value, $expiration) {
        $this->values [$key] = array ('value'   => $value,
                                      'expires' => time () + $expiration);
    }

    public function get ($key) {
        if (isset ($this->values [$key])) {
            if ($this->values [$key] ['expires'] < time ()) {
                unset ($this->values [$key]);
                return null;
            }
            return $this->values [$key] ['value'];
        }
        else
            return null;
    }
}

$storage = new Storage ();

$server->on (Memcached::ON_GET,
             function ($client_id, $key, &$value, &$flags, &$cas) use ($storage) {
                 echo "Getting key=[$key]" . PHP_EOL;
                 if (($value = $storage->get ($key)) != null)
                     return Memcached::RESPONSE_SUCCESS;

                 return Memcached::RESPONSE_KEY_ENOENT;
             });

$server->on (Memcached::ON_SET,
             function ($client_id, $key, $value, $flags, $expiration, $cas, &$result_cas) use ($storage) {
                 echo "Setting key=[$key] value=[$value]" . PHP_EOL;
                 $storage->set ($key, $value, $expiration);
                 return Memcached::RESPONSE_SUCCESS;
             });

$server->run ("127.0.0.1:3434");