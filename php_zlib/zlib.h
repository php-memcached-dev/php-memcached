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
*/

#include <zlib.h>

int zlib_compress_level(char *dest, size_t *destLen, const char *source, size_t sourceLen, int level);

int zlib_compress(char *dest, size_t *destLen, const char *source, size_t sourceLen);

int zlib_uncompress(char *dest, size_t *destLen, const char *source, size_t sourceLen);
