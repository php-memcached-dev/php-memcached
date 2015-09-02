#!/bin/bash

function version_compare() {
    DPKG=`which dpkg`

    if [ "x$DPKG" = "x" ]; then
        echo "dpkg not found, cannot compare versions"
        exit 1
    fi

    $DPKG --compare-versions "$1" "$2" "$3"
    return $?
}

function check_protocol_support() {
    version_compare "$LIBMEMCACHED_VERSION" gt 1.0.15
    if [ $? = 0 ]; then
        ENABLE_PROTOOCOL=yes
    else
        ENABLE_PROTOOCOL=no
    fi
}

function check_sasl_support() {
    version_compare "$LIBMEMCACHED_VERSION" gt 1.0.15
    if [ $? = 0 ]; then
        ENABLE_SASL=yes
    else
        ENABLE_SASL=no
    fi
}

function validate_package_xml() {
    retval=0
    for file in tests/*.phpt; do
        grep $(basename $file) package.xml >/dev/null
        if [ $? != 0 ]; then
            echo "Missing $file from package.xml"
            retval=1;
        fi
    done
    return $retval
}

function install_libmemcached() {

    wget "https://launchpad.net/libmemcached/1.0/${LIBMEMCACHED_VERSION}/+download/libmemcached-${LIBMEMCACHED_VERSION}.tar.gz" -O libmemcached-${LIBMEMCACHED_VERSION}.tar.gz

    tar xvfz libmemcached-${LIBMEMCACHED_VERSION}.tar.gz
    pushd "libmemcached-${LIBMEMCACHED_VERSION}"

        local protocol_flag=""
        if test "x$ENABLE_PROTOOCOL" = "xyes"; then
            protocol_flag="--enable-libmemcachedprotocol"
        fi

        ./configure --prefix="$LIBMEMCACHED_PREFIX" $protocol_flag LDFLAGS="-lpthread"
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
		git checkout php7
        phpize
        ./configure
        make
        make install
    popd
}

function install_sasl() {

    wget http://memcached.googlecode.com/files/memcached-1.4.15.tar.gz -O memcached-1.4.15.tar.gz
    tar xfz memcached-1.4.15.tar.gz

    pushd memcached-1.4.15
        ./configure --enable-sasl --prefix="${HOME}/memcached"
        make
        make install
    popd

    sudo apt-get install sasl2-bin
    export SASL_CONF_PATH="${HOME}/sasl2"

    # Create config path
    mkdir "${SASL_CONF_PATH}"

    # Create configuration
    cat<<EOF > "${SASL_CONF_PATH}/memcached.conf"
mech_list: PLAIN
plainlog_level: 5
sasldb_path: ${SASL_CONF_PATH}/sasldb2
EOF

    # Create password
    echo "test" | /usr/sbin/saslpasswd2 -c memcached -a memcached -f "${SASL_CONF_PATH}/sasldb2"

    # Run memcached on port 11212 with SASL support
    "${HOME}/memcached/bin/memcached" -S -d -p 11212
}

function build_php_memcached() {
    pear package
    mkdir "$PHP_MEMCACHED_BUILD_DIR"
    tar xfz "memcached-${PHP_MEMCACHED_VERSION}.tgz" -C "$PHP_MEMCACHED_BUILD_DIR"
    pushd "${PHP_MEMCACHED_BUILD_DIR}/memcached-${PHP_MEMCACHED_VERSION}"
        phpize

        local protocol_flag=""
        if test "x$ENABLE_PROTOCOL" = "xyes"; then
            protocol_flag="--enable-memcached-protocol"
        fi

        local sasl_flag="--disable-memcached-sasl"
        if test "x$ENABLE_SASL" = "xyes"; then
            sasl_flag="--enable-memcached-sasl"
        fi

        # ./configure --with-libmemcached-dir="$LIBMEMCACHED_PREFIX" $protocol_flag $sasl_flag --enable-memcached-json --enable-memcached-msgpack --enable-memcached-igbinary
		./configure --with-libmemcached-dir="$LIBMEMCACHED_PREFIX" $protocol_flag $sasl_flag
		make
        make install
    popd
}

function create_memcached_test_configuration() {
cat<<EOF > "${PHP_MEMCACHED_BUILD_DIR}/memcached-${PHP_MEMCACHED_VERSION}/tests/config.inc.local"
<?php
	define ("MEMC_SERVER_HOST", "127.0.0.1");
	define ("MEMC_SERVER_PORT", 11211);

	define ("MEMC_SASL_SERVER_HOST", "127.0.0.1");
	define ("MEMC_SASL_SERVER_PORT", 11212);
    
	define ('MEMC_SASL_USER', 'memcached');
	define ('MEMC_SASL_PASS', 'test');
EOF
}

function run_memcached_tests() {
    export NO_INTERACTION=1
    export REPORT_EXIT_STATUS=1
    export TEST_PHP_EXECUTABLE=`which php`

    pushd "${PHP_MEMCACHED_BUILD_DIR}/memcached-${PHP_MEMCACHED_VERSION}"
        # We have one xfail test, we run it separately
        php run-tests.php -d extension=memcached.so -n ./tests/expire.phpt
        rm ./tests/expire.phpt

        # Run normal tests
        php run-tests.php -d extension=memcached.so -n ./tests/*.phpt
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
LIBMEMCACHED_VERSION=$2

if test "x$ACTION" = "x"; then
    echo "Usage: $0 <action> <libmemcached version>"
    exit 1
fi

if test "x$LIBMEMCACHED_VERSION" = "x"; then
    echo "Usage: $0 <action> <libmemcached version>"
    exit 1
fi

# the extension version
PHP_MEMCACHED_VERSION=$(php -r '$sxe = simplexml_load_file ("package.xml"); echo (string) $sxe->version->release;')

# Libmemcached install dir
LIBMEMCACHED_PREFIX="${HOME}/libmemcached-${LIBMEMCACHED_VERSION}"

# Where to do the build
PHP_MEMCACHED_BUILD_DIR="/tmp/php-memcached-build"

# Check whether to enable building with protoocol and sasl support
check_protocol_support
check_sasl_support

echo "Enable protocol: $ENABLE_PROTOOCOL"
echo "Enable sasl: $ENABLE_SASL"

set -e

case $ACTION in
    before_script)
        # validate the package.xml
        validate_package_xml || exit 1

        # Install libmemcached version
        install_libmemcached

        # Install igbinary extension
        # install_igbinary

        # install msgpack
        # install_msgpack
        
        # install SASL
        if test "x$ENABLE_SASL" = "xyes"; then
        	install_sasl
        fi
    ;;

    script)
        # Build the extension
        build_php_memcached

        # Create configuration
        if test "x$ENABLE_SASL" = "xyes"; then
            create_memcached_test_configuration
        fi

        # Run tests
        set +e
        run_memcached_tests || exit 1
    ;;

    *)
        echo "Unknown action. Valid actions are: before_script and script"
        exit 1
    ;;
esac





