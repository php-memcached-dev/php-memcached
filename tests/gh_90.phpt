--TEST--
Test for GH #90
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php
include dirname (__FILE__) . '/config.inc';
$memcached = memc_get_instance (array (
									Memcached::OPT_BINARY_PROTOCOL => true
								));

// Create a key for use as a lock.  If this key already exists, wait till it doesn't exist.
{
    $key = 'LockKey';
    $lockToken = mt_rand(0, pow(2, 32)); //Random value betwen 0 and 2^32 for ownership verification

    while (true)
    {
        $casToken = null;
        $data = $memcached->get($key, $casToken);
        if ($memcached->getResultCode() == Memcached::RES_NOTFOUND)
        {
            if ($memcached->add($key, $lockToken, 5))
            {
                break;
            }
        }
        elseif ($data === false)
        {
            if ($memcached->cas($casToken, $key, $lockToken, 5))
            {
                break;
            }
        }

        //Sleep 10 milliseconds
        usleep(10 * 1000);
    }
}

//Do something here that requires exclusive access to this key

//Effectively delete our key lock.
{
    $casToken = null;
    if ($lockToken == $memcached->get($key, $casToken))
    {
        $memcached->cas($casToken, $key, false, 1);
    }
}

//Create 10 keys and then increment them.  The first value returned will be wrong.
{
    $keyList = array();
    for ($i = 0; $i < 10; $i++)
    {
        $keyList[] = $i . '_' . uniqid ('count_value_');
    }

    $valueList = array();
    foreach ($keyList as $key)
    {
        $valueList[$key] = $memcached->increment($key, 1, 1);
    }

    var_dump ($valueList);
}

--EXPECTF--
array(10) {
  ["0_%s"]=>
  int(1)
  ["1_%s"]=>
  int(1)
  ["2_%s"]=>
  int(1)
  ["3_%s"]=>
  int(1)
  ["4_%s"]=>
  int(1)
  ["5_%s"]=>
  int(1)
  ["6_%s"]=>
  int(1)
  ["7_%s"]=>
  int(1)
  ["8_%s"]=>
  int(1)
  ["9_%s"]=>
  int(1)
}