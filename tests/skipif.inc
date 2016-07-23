<?php
if (!extension_loaded("memcached")) {
	die("skip memcached is not loaded\n");
}

include dirname(__FILE__) . "/config.inc";

if (($m = memc_get_instance()) === NULL) {
	die ("skip can not connect to server\n");
}

if (isset($min_version)) {
	if (version_compare(memc_get_version($m), $min_version, "<")) {
		die("skip version of server pool is too old, $min_version is required\n");
	}
}
