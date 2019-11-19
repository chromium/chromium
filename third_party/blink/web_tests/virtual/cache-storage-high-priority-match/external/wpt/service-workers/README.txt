This virtual test suite executes the service worker WPT tests with the
CacheStorageHighPriorityMatch feature enabled.  This feature causes
cache.match() calls executed during a FetchEvent with a matching request
url to be prioritized over other cache_storage operations.
