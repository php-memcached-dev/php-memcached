#!/bin/bash -e

function validate_package_xml() {
    retval=0
    for file in tests/*.phpt; do
        grep $(basename $file) package.xml >/dev/null
        if [ $? != 0 ]; then
            echo "Missing $file from package.xml"
            retval=1;
        fi
    done
    return $retval;
}

function install_libmemcached() {
    local libmemcached_version=$1
    local enable_protocol=$2
    wget "https://launchpad.net/libmemcached/1.0/${libmemcached_version}/+download/libmemcached-${libmemcached_version}.tar.gz" -O libmemcached-${libmemcached_version}.tar.gz

    tar xvfz libmemcached-${libmemcached_version}.tar.gz
    pushd "libmemcached-${libmemcached_version}"

        if test "x$enable_protocol" = "xyes"; then
            protocol_flag="--enable-libmemcachedprotocol"
        fi

        ./configure --prefix="$PHP_LIBMEMCACHED_PREFIX" $protocol_flag LDFLAGS="-lpthread"
        make
        make install
    popd
}

function install_igbinary() {
    git clone https://github.com/igbinary/igbinary.git
    pushd igbinary
        phpize
        ./configure
        make
        make install
    popd
}

function build_php_memcached() {
    pear package
    mkdir /tmp/php-memcached-build
    tar xfz "memcached-${PHP_MEMCACHED_VERSION}.tgz" -C /tmp/php-memcached-build
    pushd "/tmp/php-memcached-build/memcached-${PHP_MEMCACHED_VERSION}"
        phpize

        if test "x$enable_protocol" = "xyes"; then
            protocol_flag="--enable-memcached-protocol"
        fi

        ./configure --with-libmemcached-dir="$PHP_LIBMEMCACHED_PREFIX" $protocol_flag --enable-memcached-json --enable-memcached-igbinary
        make
        make install
    popd
}

function run_memcached_tests() {
    export NO_INTERACTION=1
    export REPORT_EXIT_STATUS=1
    export TEST_PHP_EXECUTABLE=`which php`
    php run-tests.php -d extension=igbinary.so -d extension=memcached.so -n ./tests/*.phpt
    for i in `ls tests/*.out 2>/dev/null`; do echo "-- START ${i}"; cat $i; echo ""; echo "-- END"; done
}

# Command line arguments
PHP_LIBMEMCACHED_VERSION=$1

# Export the extension version
export PHP_MEMCACHED_VERSION=$(php -r '$sxe = simplexml_load_file ("package.xml"); echo (string) $sxe->version->release;')

# Libmemcached install dir
export PHP_LIBMEMCACHED_PREFIX="${HOME}/libmemcached-${PHP_LIBMEMCACHED_VERSION}"

# Check whether to enable building with protoocol support
dpkg --compare-versions "$PHP_LIBMEMCACHED_VERSION" '>' 1.0.15
if [ $? = 0 ]; then
    enable_protocol=yes
else
    enable_protocol=no
fi

echo "Enable protocol: $enable_protocol"

# validate the package.xml
validate_package_xml

# Install libmemcached version
install_libmemcached $PHP_LIBMEMCACHED_VERSION $enable_protocol

# Install igbinary extension
install_igbinary

# Build the extension
build_php_memcached $enable_protocol

# Run tests
run_memcached_tests

