This suite runs the ServiceWorker and CacheStorage tests with the
CacheStorageEagerReading feature enabled.  This feature makes
CacheStorage immediately read response bodies when cache.match()
called within a FetchEvent handler.  See crbug.com/1010624.
