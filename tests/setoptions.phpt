--TEST--
Set options using setOptions
--SKIPIF--
<?php if (!extension_loaded("memcached")) print "skip"; ?>
--FILE--
<?php 
$m = new Memcached();

/* existing options */
var_dump($m->setOptions(array(
	Memcached::OPT_PREFIX_KEY => 'a_prefix',
	Memcached::OPT_SERIALIZER => Memcached::SERIALIZER_PHP,
	Memcached::OPT_COMPRESSION => 0,
	Memcached::OPT_LIBKETAMA_COMPATIBLE => 1,
)));

var_dump($m->getOption(Memcached::OPT_PREFIX_KEY) == 'a_prefix');
var_dump($m->getOption(Memcached::OPT_SERIALIZER) == Memcached::SERIALIZER_PHP);
var_dump($m->getOption(Memcached::OPT_COMPRESSION) == 0);
var_dump($m->getOption(Memcached::OPT_LIBKETAMA_COMPATIBLE) == 1);

echo "test invalid options\n";
error_reporting(0);

var_dump($m->setOptions(array(
	"asdf" => 123
)));
echo $php_errormsg, "\n";

$php_errormsg = '';
var_dump($m->setOptions(array(
	-1 => 123
)));
echo $php_errormsg, "\n";

--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
test invalid options
bool(false)
%s::setOptions(): invalid configuration option
bool(false)
%s::setOptions(): error setting memcached option
