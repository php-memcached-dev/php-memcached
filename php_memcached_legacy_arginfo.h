/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 3f4694d4e1f3d1647a832acd8539b056b2ab5e7a */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, persistent_id)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, connection_str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getResultCode, 0, 0, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_getResultMessage arginfo_class_Memcached_getResultCode

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, cache_cb)
	ZEND_ARG_INFO(0, get_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, cache_cb)
	ZEND_ARG_INFO(0, get_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getMulti, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, get_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, get_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getDelayed, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, with_cas)
	ZEND_ARG_INFO(0, value_cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getDelayedByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, with_cas)
	ZEND_ARG_INFO(0, value_cb)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_fetch arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_fetchAll arginfo_class_Memcached_getResultCode

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_set, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_touch, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_touchByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setMulti, 0, 0, 1)
	ZEND_ARG_INFO(0, items)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, items)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_cas, 0, 0, 3)
	ZEND_ARG_INFO(0, cas_token)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_casByKey, 0, 0, 4)
	ZEND_ARG_INFO(0, cas_token)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, expiration)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_add arginfo_class_Memcached_set

#define arginfo_class_Memcached_addByKey arginfo_class_Memcached_setByKey

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_append, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_appendByKey, 0, 0, 3)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_prepend arginfo_class_Memcached_append

#define arginfo_class_Memcached_prependByKey arginfo_class_Memcached_appendByKey

#define arginfo_class_Memcached_replace arginfo_class_Memcached_set

#define arginfo_class_Memcached_replaceByKey arginfo_class_Memcached_setByKey

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_deleteMulti, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_deleteByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_deleteMultiByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_increment, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, initial_value)
	ZEND_ARG_INFO(0, expiry)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_decrement arginfo_class_Memcached_increment

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_incrementByKey, 0, 0, 2)
	ZEND_ARG_INFO(0, server_key)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, initial_value)
	ZEND_ARG_INFO(0, expiry)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_decrementByKey arginfo_class_Memcached_incrementByKey

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_addServer, 0, 0, 2)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, port)
	ZEND_ARG_INFO(0, weight)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_addServers, 0, 0, 1)
	ZEND_ARG_INFO(0, servers)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_getServerList arginfo_class_Memcached_getResultCode

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getServerByKey, 0, 0, 1)
	ZEND_ARG_INFO(0, server_key)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_resetServerList arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_quit arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_flushBuffers arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_getLastErrorMessage arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_getLastErrorCode arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_getLastErrorErrno arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_getLastDisconnectedServer arginfo_class_Memcached_getResultCode

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getStats, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

#define arginfo_class_Memcached_getVersion arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_getAllKeys arginfo_class_Memcached_getResultCode

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_flush, 0, 0, 0)
	ZEND_ARG_INFO(0, delay)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_getOption, 0, 0, 1)
	ZEND_ARG_INFO(0, option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setOption, 0, 0, 2)
	ZEND_ARG_INFO(0, option)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setOptions, 0, 0, 1)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setBucket, 0, 0, 3)
	ZEND_ARG_INFO(0, host_map)
	ZEND_ARG_INFO(0, forward_map)
	ZEND_ARG_INFO(0, replicas)
ZEND_END_ARG_INFO()

#if defined(HAVE_MEMCACHED_SASL)
ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setSaslAuthData, 0, 0, 2)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()
#endif

#if defined(HAVE_MEMCACHED_SET_ENCODING_KEY)
ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_setEncodingKey, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()
#endif

#define arginfo_class_Memcached_isPersistent arginfo_class_Memcached_getResultCode

#define arginfo_class_Memcached_isPristine arginfo_class_Memcached_getResultCode

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Memcached_checkKey, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

#if defined(HAVE_MEMCACHED_PROTOCOL)
ZEND_BEGIN_ARG_INFO_EX(arginfo_class_MemcachedServer_run, 0, 0, 1)
	ZEND_ARG_INFO(0, address)
ZEND_END_ARG_INFO()
#endif

#if defined(HAVE_MEMCACHED_PROTOCOL)
ZEND_BEGIN_ARG_INFO_EX(arginfo_class_MemcachedServer_on, 0, 0, 2)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()
#endif


ZEND_METHOD(Memcached, __construct);
ZEND_METHOD(Memcached, getResultCode);
ZEND_METHOD(Memcached, getResultMessage);
ZEND_METHOD(Memcached, get);
ZEND_METHOD(Memcached, getByKey);
ZEND_METHOD(Memcached, getMulti);
ZEND_METHOD(Memcached, getMultiByKey);
ZEND_METHOD(Memcached, getDelayed);
ZEND_METHOD(Memcached, getDelayedByKey);
ZEND_METHOD(Memcached, fetch);
ZEND_METHOD(Memcached, fetchAll);
ZEND_METHOD(Memcached, set);
ZEND_METHOD(Memcached, setByKey);
ZEND_METHOD(Memcached, touch);
ZEND_METHOD(Memcached, touchByKey);
ZEND_METHOD(Memcached, setMulti);
ZEND_METHOD(Memcached, setMultiByKey);
ZEND_METHOD(Memcached, cas);
ZEND_METHOD(Memcached, casByKey);
ZEND_METHOD(Memcached, add);
ZEND_METHOD(Memcached, addByKey);
ZEND_METHOD(Memcached, append);
ZEND_METHOD(Memcached, appendByKey);
ZEND_METHOD(Memcached, prepend);
ZEND_METHOD(Memcached, prependByKey);
ZEND_METHOD(Memcached, replace);
ZEND_METHOD(Memcached, replaceByKey);
ZEND_METHOD(Memcached, delete);
ZEND_METHOD(Memcached, deleteMulti);
ZEND_METHOD(Memcached, deleteByKey);
ZEND_METHOD(Memcached, deleteMultiByKey);
ZEND_METHOD(Memcached, increment);
ZEND_METHOD(Memcached, decrement);
ZEND_METHOD(Memcached, incrementByKey);
ZEND_METHOD(Memcached, decrementByKey);
ZEND_METHOD(Memcached, addServer);
ZEND_METHOD(Memcached, addServers);
ZEND_METHOD(Memcached, getServerList);
ZEND_METHOD(Memcached, getServerByKey);
ZEND_METHOD(Memcached, resetServerList);
ZEND_METHOD(Memcached, quit);
ZEND_METHOD(Memcached, flushBuffers);
ZEND_METHOD(Memcached, getLastErrorMessage);
ZEND_METHOD(Memcached, getLastErrorCode);
ZEND_METHOD(Memcached, getLastErrorErrno);
ZEND_METHOD(Memcached, getLastDisconnectedServer);
ZEND_METHOD(Memcached, getStats);
ZEND_METHOD(Memcached, getVersion);
ZEND_METHOD(Memcached, getAllKeys);
ZEND_METHOD(Memcached, flush);
ZEND_METHOD(Memcached, getOption);
ZEND_METHOD(Memcached, setOption);
ZEND_METHOD(Memcached, setOptions);
ZEND_METHOD(Memcached, setBucket);
#if defined(HAVE_MEMCACHED_SASL)
ZEND_METHOD(Memcached, setSaslAuthData);
#endif
#if defined(HAVE_MEMCACHED_SET_ENCODING_KEY)
ZEND_METHOD(Memcached, setEncodingKey);
#endif
ZEND_METHOD(Memcached, isPersistent);
ZEND_METHOD(Memcached, isPristine);
ZEND_METHOD(Memcached, checkKey);
#if defined(HAVE_MEMCACHED_PROTOCOL)
ZEND_METHOD(MemcachedServer, run);
#endif
#if defined(HAVE_MEMCACHED_PROTOCOL)
ZEND_METHOD(MemcachedServer, on);
#endif


static const zend_function_entry class_Memcached_methods[] = {
	ZEND_ME(Memcached, __construct, arginfo_class_Memcached___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getResultCode, arginfo_class_Memcached_getResultCode, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getResultMessage, arginfo_class_Memcached_getResultMessage, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, get, arginfo_class_Memcached_get, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getByKey, arginfo_class_Memcached_getByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getMulti, arginfo_class_Memcached_getMulti, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getMultiByKey, arginfo_class_Memcached_getMultiByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getDelayed, arginfo_class_Memcached_getDelayed, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getDelayedByKey, arginfo_class_Memcached_getDelayedByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, fetch, arginfo_class_Memcached_fetch, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, fetchAll, arginfo_class_Memcached_fetchAll, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, set, arginfo_class_Memcached_set, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, setByKey, arginfo_class_Memcached_setByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, touch, arginfo_class_Memcached_touch, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, touchByKey, arginfo_class_Memcached_touchByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, setMulti, arginfo_class_Memcached_setMulti, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, setMultiByKey, arginfo_class_Memcached_setMultiByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, cas, arginfo_class_Memcached_cas, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, casByKey, arginfo_class_Memcached_casByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, add, arginfo_class_Memcached_add, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, addByKey, arginfo_class_Memcached_addByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, append, arginfo_class_Memcached_append, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, appendByKey, arginfo_class_Memcached_appendByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, prepend, arginfo_class_Memcached_prepend, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, prependByKey, arginfo_class_Memcached_prependByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, replace, arginfo_class_Memcached_replace, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, replaceByKey, arginfo_class_Memcached_replaceByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, delete, arginfo_class_Memcached_delete, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, deleteMulti, arginfo_class_Memcached_deleteMulti, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, deleteByKey, arginfo_class_Memcached_deleteByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, deleteMultiByKey, arginfo_class_Memcached_deleteMultiByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, increment, arginfo_class_Memcached_increment, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, decrement, arginfo_class_Memcached_decrement, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, incrementByKey, arginfo_class_Memcached_incrementByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, decrementByKey, arginfo_class_Memcached_decrementByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, addServer, arginfo_class_Memcached_addServer, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, addServers, arginfo_class_Memcached_addServers, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getServerList, arginfo_class_Memcached_getServerList, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getServerByKey, arginfo_class_Memcached_getServerByKey, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, resetServerList, arginfo_class_Memcached_resetServerList, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, quit, arginfo_class_Memcached_quit, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, flushBuffers, arginfo_class_Memcached_flushBuffers, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getLastErrorMessage, arginfo_class_Memcached_getLastErrorMessage, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getLastErrorCode, arginfo_class_Memcached_getLastErrorCode, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getLastErrorErrno, arginfo_class_Memcached_getLastErrorErrno, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getLastDisconnectedServer, arginfo_class_Memcached_getLastDisconnectedServer, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getStats, arginfo_class_Memcached_getStats, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getVersion, arginfo_class_Memcached_getVersion, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getAllKeys, arginfo_class_Memcached_getAllKeys, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, flush, arginfo_class_Memcached_flush, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, getOption, arginfo_class_Memcached_getOption, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, setOption, arginfo_class_Memcached_setOption, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, setOptions, arginfo_class_Memcached_setOptions, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, setBucket, arginfo_class_Memcached_setBucket, ZEND_ACC_PUBLIC)
#if defined(HAVE_MEMCACHED_SASL)
	ZEND_ME(Memcached, setSaslAuthData, arginfo_class_Memcached_setSaslAuthData, ZEND_ACC_PUBLIC)
#endif
#if defined(HAVE_MEMCACHED_SET_ENCODING_KEY)
	ZEND_ME(Memcached, setEncodingKey, arginfo_class_Memcached_setEncodingKey, ZEND_ACC_PUBLIC)
#endif
	ZEND_ME(Memcached, isPersistent, arginfo_class_Memcached_isPersistent, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, isPristine, arginfo_class_Memcached_isPristine, ZEND_ACC_PUBLIC)
	ZEND_ME(Memcached, checkKey, arginfo_class_Memcached_checkKey, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};


static const zend_function_entry class_MemcachedServer_methods[] = {
#if defined(HAVE_MEMCACHED_PROTOCOL)
	ZEND_ME(MemcachedServer, run, arginfo_class_MemcachedServer_run, ZEND_ACC_PUBLIC)
#endif
#if defined(HAVE_MEMCACHED_PROTOCOL)
	ZEND_ME(MemcachedServer, on, arginfo_class_MemcachedServer_on, ZEND_ACC_PUBLIC)
#endif
	ZEND_FE_END
};
