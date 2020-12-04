// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
const metadataProxy = {};

/**
 * Maximum number of entries whose metadata can be cached.
 * @const {number}
 * @private
 */
metadataProxy.MAX_CACHED_METADATA_ = 10000;

/**
 * Maximum time for entry to not be considered stale.
 * @const {number}
 * @private
 */
metadataProxy.MAX_TTL_SECONDS_ = 60;

/**
 * @type {number}
 * @private
 */
metadataProxy.cache_ttl_seconds_ = metadataProxy.MAX_TTL_SECONDS_;

/**
 * Cached metadata element with a timestamp when it was fetched.
 */
class CachedMetadata {
  /**
   * @param {!Metadata} metadata
   */
  constructor(metadata) {
    /** @type {!Metadata} */
    this.metadata = metadata;

    /** @type {!Date}} */
    this.mtime = new Date();
  }
}

/**
 * @private {!LRUCache<!CachedMetadata>}
 */
metadataProxy.cache_ = new LRUCache(metadataProxy.MAX_CACHED_METADATA_);

/**
 * Returns metadata for the given FileEntry. Uses cached metadata if possible.
 *
 * @param {!FileEntry} entry
 * @return {!Promise<!Metadata>}
 */
metadataProxy.getEntryMetadata = entry => {
  const expiryCutoffTime = Date.now() - metadataProxy.cache_ttl_seconds_ * 1000;
  const entryURL = entry.toURL();
  const cachedData = metadataProxy.cache_.get(entryURL);
  if (cachedData && cachedData.mtime.getTime() >= expiryCutoffTime) {
    return Promise.resolve(cachedData.metadata);
  } else {
    return new Promise((resolve, reject) => {
      entry.getMetadata(metadata => {
        metadataProxy.cache_.put(entryURL, new CachedMetadata(metadata));
        resolve(metadata);
      }, reject);
    });
  }
};

/**
 * Override cache TTL for testing.
 *
 * @param {number=} ttl
 */
metadataProxy.overrideCacheTtlForTesting = ttl => {
  metadataProxy.cache_ttl_seconds_ = ttl ? ttl : metadataProxy.MAX_TTL_SECONDS_;
};
