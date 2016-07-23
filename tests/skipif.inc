<?php
if (!extension_loaded("memcached")) {
	die("skip memcached is not loaded\n");
}

include dirname(__FILE__) . "/config.inc";

if (memc_get_instance() === NULL) {
	die ("skip can not connect to server\n");
}
