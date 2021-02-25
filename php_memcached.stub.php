<?php

/**
 * @generate-function-entries
 * @generate-legacy-arginfo
 */


class Memcached {

	public function __construct(?string $persistent_id=null, ?callable $callback=null, ?string $connection_str=null) {}

	public function getResultCode(): int {}
	public function getResultMessage(): string {}

	public function get(string $key, ?callable $cache_cb=null, int $get_flags=0): mixed {}
	public function getByKey(string $server_key, string $key, ?callable $cache_cb=null, int $get_flags=0): mixed {}
	public function getMulti(array $keys, int $get_flags=0): false|array {}
	public function getMultiByKey(string $server_key, array $keys, int $get_flags=0): false|array {}
	public function getDelayed(array $keys, bool $with_cas=false, ?callable $value_cb=null): bool {}
	public function getDelayedByKey(string $server_key, array $keys, bool $with_cas=false, ?callable $value_cb=null): bool {}
	public function fetch(): false|array {}
	public function fetchAll(): false|array {}

	public function set(string $key, mixed $value, int $expiration=0): bool {}
	public function setByKey(string $server_key, string $key, mixed $value, int $expiration=0): bool {}

	public function touch(string $key, int $expiration=0): bool {}
	public function touchByKey(string $server_key, string $key, int $expiration=0): bool {}

	public function setMulti(array $items, int $expiration=0): bool {}
	public function setMultiByKey(string $server_key, array $items, int $expiration=0): bool {}

	public function cas(string $cas_token, string $key, mixed $value, int $expiration=0): bool {}
	public function casByKey(string $cas_token, string $server_key, string $key, mixed $value, int $expiration=0): bool {}
	public function add(string $key, mixed $value, int $expiration=0): bool {}
	public function addByKey(string $server_key, string $key, mixed $value, int $expiration=0): bool {}
	public function append(string $key, string $value): ?bool {}
	public function appendByKey(string $server_key, string $key, string $value): ?bool {}
	public function prepend(string $key, string $value): ?bool {}
	public function prependByKey(string $server_key, string $key, string $value): ?bool {}
	public function replace(string $key, mixed $value, int $expiration=0): bool {}
	public function replaceByKey(string $server_key, string $key, mixed $value, int $expiration=0): bool {}
	public function delete(string $key, int $time=0): bool {}
	public function deleteMulti(array $keys, int $time=0): array {}
	public function deleteByKey(string $server_key, string $key, int $time=0): bool {}
	public function deleteMultiByKey(string $server_key, array $keys, int $time=0): array {}

	public function increment(string $key, int $offset=1, int $initial_value=0, int $expiry=0): false|int {}
	public function decrement(string $key, int $offset=1, int $initial_value=0, int $expiry=0): false|int {}
	public function incrementByKey(string $server_key, string $key, int $offset=1, int $initial_value=0, int $expiry=0): false|int {}
	public function decrementByKey(string $server_key, string $key, int $offset=1, int $initial_value=0, int $expiry=0): false|int {}

	public function addServer(string $host, int $port, int $weight=0): bool {}
	public function addServers(array $servers): bool {}
	public function getServerList(): array {}
	public function getServerByKey(string $server_key): false|array {}
	public function resetServerList(): bool {}
	public function quit(): bool {}
	public function flushBuffers(): bool {}

	public function getLastErrorMessage(): string {}
	public function getLastErrorCode(): int {}
	public function getLastErrorErrno(): int {}
	public function getLastDisconnectedServer(): false|array {}

	public function getStats(?string $type=null): false|array {}
	public function getVersion(): false|array {}
	public function getAllKeys(): false|array {}

	public function flush(int $delay=0): bool {}

	public function getOption(int $option): mixed {}
	public function setOption(int $option, mixed $value): bool {}
	public function setOptions(array $options): bool {}
	public function setBucket(array $host_map, ?array $forward_map, int $replicas): bool {}
#ifdef HAVE_MEMCACHED_SASL
	public function setSaslAuthData(string $username, string $password): bool {}
#endif

#ifdef HAVE_MEMCACHED_SET_ENCODING_KEY
	public function setEncodingKey(string $key): bool {}
#endif
	public function isPersistent(): bool {}
	public function isPristine(): bool {}
	public function checkKey(string $key): bool {}
}

#ifdef HAVE_MEMCACHED_PROTOCOL
class MemcachedServer {

	public function run(string $address): bool {}
	public function on(int $event, callable $callback): bool {}
}
#endif
