/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2009-2010 The PHP Group                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is    |
  | available through the world-wide-web at the following url:       |
  | http://www.php.net/license/3_01.txt.                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to      |
  | license@php.net so we can mail you a copy immediately.           |
  +----------------------------------------------------------------------+
*/

#include "php.h"
#include "zlib.h"

/* This code is copy from "gzcompress" function. */
static voidpf php_zlib_alloc(voidpf opaque, uInt items, uInt size)
{
    return (voidpf)safe_emalloc(items, size, 0);
}

/* This code is copy from "gzcompress" function. */
static void php_zlib_free(voidpf opaque, voidpf address)
{
    efree((void*)address);
}

int zlib_compress_level(char *dest, size_t *destLen, const char *source, size_t sourceLen, int level)
{
    z_stream stream;
    int err;

    stream.next_in = (const Bytef *)source;
    stream.avail_in = (uInt)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    stream.zalloc = php_zlib_alloc;
    stream.zfree = php_zlib_free;

    if ((err = deflateInit(&stream, level)) != Z_OK) {
        return err;
    }

    if ((err = deflate(&stream, Z_FINISH)) != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}

int zlib_compress(char *dest, size_t *destLen, const char *source, size_t sourceLen)
{
    return zlib_compress_level(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

int zlib_uncompress(char *dest, size_t *destLen, const char *source, size_t sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (const Bytef *)source;
    stream.avail_in = (uInt)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    stream.zalloc = (alloc_func)php_zlib_alloc;
    stream.zfree = (free_func)php_zlib_free;

    if ((err = inflateInit(&stream)) != Z_OK) {
        return err;
    }

    if ((err = inflate(&stream, Z_FINISH)) != Z_STREAM_END) {
        inflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}
