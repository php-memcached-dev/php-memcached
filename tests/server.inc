<?php
function memcached_server_start($code = 'server.php', $host = "127.0.0.1", $port = 3434) {
	$php_executable = getenv('TEST_PHP_EXECUTABLE') ?: PHP_BINARY;
	$php_args = getenv('TEST_PHP_ARGS') ?: '';

	$descriptorspec = array(
		0 => STDIN,
		1 => STDOUT,
		2 => STDERR,
	);

	$cmd = "{$php_executable} {$php_args} {$code} {$host}:{$port} ";
	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$cmd = "{$php_executable} {$php_args} {$code} {$host}:{$port} ";

		$handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, __DIR__, NULL, array("bypass_shell" => true,  "suppress_errors" => true));
	} else {
		$cmd = "exec {$cmd} 2>/dev/null";

		$handle = proc_open($cmd, $descriptorspec, $pipes, __DIR__);
	}

    // note: even when server prints 'Listening on localhost:8964...Press Ctrl-C to quit.'
    //       it might not be listening yet...need to wait until fsockopen() call returns
    $error = "Unable to connect to server\n";
    for ($i=0; $i < getenv("VALGRIND") ? 1000 : 60; $i++) {
        usleep(50000); // 50ms per try
        $status = proc_get_status($handle);
        $fp = @fsockopen($host, $port);
        // Failure, the server is no longer running
        if (!($status && $status['running'])) {
            $error = "Server is not running {$status['command']}\n";
            break;
        }
        // Success, Connected to servers
        if ($fp) {
            $error = '';
            break;
        }
    }

    if ($fp) {
        fclose($fp);
    }

    if ($error) {
        echo $error;
        proc_terminate($handle);
        proc_close($handle);
        exit(1);
    }

    register_shutdown_function(
        function($handle) {
            if (is_resource($handle)) {
                proc_terminate($handle);
                proc_close($handle);
            }
        },
        $handle
    );

    return $handle;
}

function memcached_server_stop($handle) {
    $success = FALSE;
    if ($handle) {
        proc_terminate($handle);
        /* Wait for server to shutdown */
        for ($i = 0; $i < 60; $i++) {
            $status = proc_get_status($handle);
            if (!($status && $status['running'])) {
                $success = TRUE;
                break;
            }
            usleep(50000);
        }
        proc_close($handle);
    }
    return $success;
}
