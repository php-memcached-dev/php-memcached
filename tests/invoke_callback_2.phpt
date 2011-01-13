--TEST--
Use callback initializer
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 

$arg = 'callback_arg';
$id = 'my_persistent_id';

function init_cb($m, $id) {
	var_dump(get_class($m));
	var_dump($m->isPersistent());
	var_dump($id);
}

function init_cb_fail($m, $id) {
	echo "configured, should not be called.\n";
}

function init_cb_arg($m, $id, $arg) {
	var_dump($id);
	var_dump($arg);
}

function init_nopersist_cb($m, $id) {
	var_dump($m->isPersistent());
	var_dump($id);
}

class Foo extends Memcached {
	function __construct($id = null) {
		parent::__construct($id, array($this, 'init'));
	}

	function init($obj, $id, $options) {
		var_dump($this->isPristine());
		var_dump($this->isPersistent());
		var_dump($id);
	}
}

error_reporting(0);

echo "cb call\n";
$m1 = new Memcached('foo1', 'init_cb');

echo "cb not run\n";
$m1 = new Memcached('foo1', 'init_cb_fail');

echo "cb arg without arg\n";
$m1 = new Memcached('foo3', 'init_cb_arg');
echo $php_errormsg, "\n";

echo "cb arg not persistent\n";
$m1 = new Memcached(null, 'init_nopersist_cb');

echo "cb in object\n";
$m1 = new Foo();

echo "cb persistent in object\n";
$m1 = new Foo('baz');

echo "cb second persistent in object\n";
$m1 = new Foo('baz');

--EXPECT--
cb call
string(9) "Memcached"
bool(true)
string(4) "foo1"
cb not run
cb arg without arg
string(4) "foo3"
NULL

cb arg not persistent
bool(false)
NULL
cb in object
bool(true)
bool(false)
NULL
cb persistent in object
bool(true)
bool(true)
string(3) "baz"
cb second persistent in object
