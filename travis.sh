#!/bin/bash

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
    local libmemcached_prefix=$2
    local enable_protocol=$3
    
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

function install_msgpack() {
    git clone https://github.com/msgpack/msgpack-php.git
    pushd msgpack-php
        phpize
        ./configure
        make
        make install
    popd
}

function build_php_memcached() {
    local libmemcached_prefix=$1
    local php_memcached_version=$2
    local enable_protocol=$3

    pear package
    mkdir /tmp/php-memcached-build
    tar xfz "memcached-${php_memcached_version}.tgz" -C /tmp/php-memcached-build
    pushd "/tmp/php-memcached-build/memcached-${php_memcached_version}"
        phpize

        if test "x$enable_protocol" = "xyes"; then
            protocol_flag="--enable-memcached-protocol"
        fi

        ./configure --with-libmemcached-dir="$libmemcached_prefix" $protocol_flag --enable-memcached-json --enable-memcached-igbinary --enable-memcached-msgpack
        make
        make install
    popd
}

function run_memcached_tests() {
    local php_memcached_version=$1

    export NO_INTERACTION=1
    export REPORT_EXIT_STATUS=1
    export TEST_PHP_EXECUTABLE=`which php`

    pushd "/tmp/php-memcached-build/memcached-${php_memcached_version}"
        # We have one xfail test, we run it separately
        php run-tests.php -d extension=msgpack.so -d extension=igbinary.so -d extension=memcached.so -n ./tests/expire.phpt
        rm ./tests/expire.phpt

        php run-tests.php -d extension=msgpack.so -d extension=igbinary.so -d extension=memcached.so -n ./tests/*.phpt
        retval=$?
        for i in `ls tests/*.out 2>/dev/null`; do
            echo "-- START ${i}";
            cat $i;
            echo "";
            echo "-- END";
        done
    popd

    return $retval;
}

# Command line arguments
ACTION=$1
PHP_LIBMEMCACHED_VERSION=$2

if test "x$ACTION" = "x"; then
    echo "Usage: $0 <action> <libmemcached version>"
    exit 1
fi

if test "x$PHP_LIBMEMCACHED_VERSION" = "x"; then
    echo "Usage: $0 <action> <libmemcached version>"
    exit 1
fi

# the extension version
PHP_MEMCACHED_VERSION=$(php -r '$sxe = simplexml_load_file ("package.xml"); echo (string) $sxe->version->release;')

# Libmemcached install dir
PHP_LIBMEMCACHED_PREFIX="${HOME}/libmemcached-${PHP_LIBMEMCACHED_VERSION}"

# Check whether to enable building with protoocol support
DPKG=`which dpkg`

if [ "x$DPKG" = "x" ]; then
    echo "dpkg not found, enabling protocol support"
    ENABLE_PROTOOCOL=yes
else
    dpkg --compare-versions "$PHP_LIBMEMCACHED_VERSION" gt 1.0.15
    if [ $? = 0 ]; then
        ENABLE_PROTOOCOL=yes
    else
        ENABLE_PROTOOCOL=no
    fi
fi

echo "Enable protocol: $ENABLE_PROTOOCOL"

set -e

case $ACTION in
    before_script)
        # validate the package.xml
        validate_package_xml || exit 1

        # Install libmemcached version
        install_libmemcached $PHP_LIBMEMCACHED_VERSION $PHP_LIBMEMCACHED_PREFIX $ENABLE_PROTOOCOL

        # Install igbinary extension
        install_igbinary

        # install msgpack
        install_msgpack
    ;;

    script)
        # Build the extension
        build_php_memcached $PHP_LIBMEMCACHED_PREFIX $PHP_MEMCACHED_VERSION $ENABLE_PROTOOCOL

        # Run tests
        set +e
        run_memcached_tests $PHP_MEMCACHED_VERSION || exit 1
    ;;

    *)
        echo "Unknown action. Valid actions are: before_script and script"
        exit 1
    ;;
esac





