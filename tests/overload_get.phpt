--TEST--
Test overloading get-method
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
class test_overload_get extends Memcached
{
  public function get($key, $cache_cb = null, &$cas_token = null)
  {
    return parent::get($key, $cache_cb, $cas_token);
  }
}
echo "GOT HERE";
--EXPECTF--
GOT HERE
