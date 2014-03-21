<?php

/**
 * Memcached class.
 */

class Memcached {
	/**
	 * Libmemcached behavior options.
	 */

	const OPT_HASH;
	
	const OPT_HASH_DEFAULT;

	const HASH_MD5;

	const HASH_CRC;
	
	const HASH_FNV1_64;

	const HASH_FNV1A_64;

	const HASH_FNV1_32;

	const HASH_FNV1A_32;

	const HASH_HSIEH;

	const HASH_MURMUR;

	const OPT_DISTRIBUTION;

	const DISTRIBUTION_MODULA;

	const DISTRIBUTION_CONSISTENT;
	
	const DISTRIBUTION_VIRTUAL_BUCKET;

	const LIBKETAMA_COMPATIBLE;

	const OPT_BUFFER_REQUESTS;

	const OPT_BINARY_PROTOCOL;

	const OPT_NO_BLOCK;

	const OPT_TCP_NODELAY;

	const OPT_SOCKET_SEND_SIZE;

	const OPT_SOCKET_RECV_SIZE;

	const OPT_CONNECT_TIMEOUT;

	const OPT_RETRY_TIMEOUT;

	const OPT_DEAD_TIMEOUT;

	const OPT_SND_TIMEOUT;

	const OPT_RCV_TIMEOUT;

	const OPT_POLL_TIMEOUT;

	const OPT_SERVER_FAILURE_LIMIT;

	const OPT_SERVER_TIMEOUT_LIMIT;

	const OPT_CACHE_LOOKUPS;

	const OPT_AUTO_EJECT_HOSTS;

	const OPT_NUMBER_OF_REPLICAS;

	const OPT_NOREPLY;

	const OPT_VERIFY_KEY;
	
	const OPT_RANDOMIZE_REPLICA_READS;


	/**
	 * Class parameters
	 */
	const HAVE_JSON;

	const HAVE_IGBINARY;

	/**
	 * Class options.
	 */
	const OPT_COMPRESSION;

	const OPT_COMPRESSION_TYPE;

	const OPT_PREFIX_KEY;

	/**
	 * Serializer constants
	 */
	const SERIALIZER_PHP;

	const SERIALIZER_IGBINARY;

	const SERIALIZER_JSON;

	const SERIALIZER_JSON_ARRAY;

	/**
	 * Compression types
	 */
	const COMPRESSION_TYPE_FASTLZ;

	const COMPRESSION_TYPE_ZLIB;

	/**
	 * Flags
	 */
	const GET_PRESERVE_ORDER;

	/**
	 * Return values
	 */
	const GET_ERROR_RETURN_VALUE;

	const RES_PAYLOAD_FAILURE;

	const RES_SUCCESS;

	const RES_FAILURE;

	const RES_HOST_LOOKUP_FAILURE;

	const RES_UNKNOWN_READ_FAILURE;

	const RES_PROTOCOL_ERROR;

	const RES_CLIENT_ERROR;

	const RES_SERVER_ERROR;

	const RES_WRITE_FAILURE;

	const RES_DATA_EXISTS;

	const RES_NOTSTORED;

	const RES_NOTFOUND;

	const RES_PARTIAL_READ;

	const RES_SOME_ERRORS;

	const RES_NO_SERVERS;

	const RES_END;

	const RES_ERRNO;

	const RES_BUFFERED;

	const RES_TIMEOUT;

	const RES_BAD_KEY_PROVIDED;

	const RES_STORED;

	const RES_DELETED;

	const RES_STAT;

	const RES_ITEM;

	const RES_NOT_SUPPORTED;

	const RES_FETCH_NOTFINISHED;

	const RES_SERVER_MARKED_DEAD;

	const RES_UNKNOWN_STAT_KEY;

	const RES_INVALID_HOST_PROTOCOL;

	const RES_MEMORY_ALLOCATION_FAILURE;

	const RES_CONNECTION_SOCKET_CREATE_FAILURE;


	public function __construct( $persistent_id = '', $on_new_object_cb = null ) {}
	
	public function get( $key, $cache_cb = null, &$cas_token = null, &$udf_flags = null ) {}

	public function getByKey( $server_key, $key, $cache_cb = null, &$cas_token = null, &$udf_flags = null ) {}

	public function getMulti( array $keys, &$cas_tokens = null, $flags = 0, &$udf_flags = null ) {}

	public function getMultiByKey( $server_key, array $keys, &$cas_tokens = null, $flags = 0, &$udf_flags = null ) {}

	public function getDelayed( array $keys, $with_cas = null, $value_cb = null ) {}

	public function getDelayedByKey( $server_key, array $keys, $with_cas = null, $value_cb = null ) {}

	public function fetch( ) {}
	
	public function fetchAll( ) {}

	public function set( $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function touch( $key, $expiration = 0 ) {}

	public function touchbyKey( $key, $expiration = 0 ) {}

	public function setByKey( $server_key, $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function setMulti( array $items, $expiration = 0, $udf_flags = 0 ) {}

	public function setMultiByKey( $server_key, array $items, $expiration = 0, $udf_flags = 0 ) {}

	public function cas( $token, $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function casByKey( $token, $server_key, $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function add( $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function addByKey( $server_key, $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function append( $key, $value ) {}

	public function appendByKey( $server_key, $key, $value ) {}

	public function prepend( $key, $value ) {}

	public function prependByKey( $server_key, $key, $value ) {}

	public function replace( $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function replaceByKey( $server_key, $key, $value, $expiration = 0, $udf_flags = 0 ) {}

	public function delete( $key, $time = 0 ) {}

	public function deleteByKey( $server_key, $key, $time = 0 ) {}

	public function deleteMulti( array $keys, $expiration = 0 ) {}

	public function deleteMultiByKey( $server_key, array $keys, $expiration = 0 ) {}

	public function increment( $key, $offset = 1, $initial_value = 0, $expiry = 0) {}

	public function decrement( $key, $offset = 1, $initial_value = 0, $expiry = 0) {}

	public function getOption( $option ) {}
	
	public function setOption( $option, $value ) {}

	public function setOptions( array $options ) {}

	public function setBucket( array $host_map, array $forward_map, $replicas ) {}

	public function addServer( $host, $port,  $weight = 0 ) {}

	public function addServers( array $servers ) {}

	public function getServerList( ) {}

	public function getServerByKey( $server_key ) {}

	public function getLastErrorMessage( ) {}

	public function getLastErrorCode( ) {}

	public function getLastErrorErrno( ) {}

	public function getLastDisconnectedServer( ) {}

	public function flush( $delay = 0 ) {}

	public function getStats( ) {}
	
	public function getVersion( ) {}

	public function getResultCode( ) {}

	public function getResultMessage( ) {}

	public function isPersistent( ) {}

	public function isPristine( ) {}

	public function setSaslAuthData( $username, $password ) {}

}

class MemcachedException extends Exception {

	function __construct( $errmsg = "", $errcode  = 0 ) {}

}
