/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2009-2010 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Andrei Zmievski <andrei@php.net>                            |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_MEMCACHED_SESSION_H
#define PHP_MEMCACHED_SESSION_H

/* session handler struct */

#include "ext/session/php_session.h"

extern ps_module ps_mod_memcached;
#define ps_memcached_ptr &ps_mod_memcached

PS_FUNCS(memcached);

PS_OPEN_FUNC(memcached);
PS_CLOSE_FUNC(memcached);
PS_READ_FUNC(memcached);
PS_WRITE_FUNC(memcached);
PS_DESTROY_FUNC(memcached);
PS_GC_FUNC(memcached);

#endif /* PHP_MEMCACHED_SESSION_H */
