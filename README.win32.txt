Build Steps for Windows
-------------------------

Follow https://wiki.php.net/internals/windows/stepbystepbuild_sdk_2#building_pecl_extensions

- Add igbinary module to pecl directory if support desired
- Download/Compile libmemcached & add to deps folders (includes & lib). Lib should be named memcache.lib
	- Important for 32bit: libmemcached must be built with _USE_32BIT_TIME_T defined (confirmed on PHP 7.2, VC15)
	- https://github.com/yshurik/libmemcached-win/tree/1.0.18 is confirmed working
	- To use the dll on the releases page you'd likely need to change the header files to use __time64_t instead of time_t
- Enable all options desired: --enable-memcached=shared --enable-memcached-session --enable-memcached-json
	- for igbinary, add --enable-memcached-igbinary --enable-igbinary=shared
- Run nmake