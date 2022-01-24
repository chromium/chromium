/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Wrapper for an IndexedDB object store.
 */


goog.provide('goog.db.ObjectStore');

goog.require('goog.async.Deferred');
goog.require('goog.db.Cursor');
goog.require('goog.db.Error');
goog.require('goog.db.Index');
goog.require('goog.db.KeyRange');
goog.require('goog.debug');



/**
 * Creates an IDBObjectStore wrapper object. Object stores have methods for
 * storing and retrieving records, and are accessed through a transaction
 * object. They also have methods for creating indexes associated with the
 * object store. They can only be created when setting the version of the
 * database. Should not be created directly, access object stores through
 * transactions.
 * @see goog.db.UpgradeNeededCallback
 * @see goog.db.Transaction#objectStore
 *
 * @param {!IDBObjectStore} store The backing IndexedDb object.
 * @constructor
 * @final
 *
 * TODO(arthurhsu): revisit msg in exception and errors in this class. In newer
 *     Chrome (v22+) the error/request come with a DOM error string that is
 *     already very descriptive.
 */
goog.db.ObjectStore = function(store) {
  'use strict';
  /**
   * Underlying IndexedDB object store object.
   *
   * @type {!IDBObjectStore}
   * @private
   */
  this.store_ = store;
};


/**
 * @return {string} The name of the object store.
 */
goog.db.ObjectStore.prototype.getName = function() {
  'use strict';
  return this.store_.name;
};


/**
 * Helper function for put and add.
 *
 * @param {string} fn Function name to call on the object store.
 * @param {string} msg Message to give to the error.
 * @param {*} value Value to insert into the object store.
 * @param {IDBKeyType=} opt_key The key to use.
 * @return {!goog.async.Deferred} The resulting deferred request.
 * @private
 */
goog.db.ObjectStore.prototype.insert_ = function(fn, msg, value, opt_key) {
  'use strict';
  // TODO(user): refactor wrapping an IndexedDB request in a Deferred by
  // creating a higher-level abstraction for it (mostly affects here and
  // goog.db.Index)
  const d = new goog.async.Deferred();
  let request;
  try {
    // put or add with (value, undefined) throws an error, so we need to check
    // for undefined ourselves
    if (opt_key) {
      request = this.store_[fn](value, opt_key);
    } else {
      request = this.store_[fn](value);
    }
  } catch (ex) {
    msg += goog.debug.deepExpose(value);
    if (opt_key) {
      msg += ', with key ' + goog.debug.deepExpose(opt_key);
    }
    d.errback(goog.db.Error.fromException(ex, msg));
    return d;
  }
  request.onsuccess = function(ev) {
    'use strict';
    d.callback(ev.target.result);
  };
  request.onerror = function(ev) {
    'use strict';
    msg += goog.debug.deepExpose(value);
    if (opt_key) {
      msg += ', with key ' + goog.debug.deepExpose(opt_key);
    }
    d.errback(goog.db.Error.fromRequest(ev.target, msg));
  };
  return d;
};


/**
 * Adds an object to the object store. Replaces existing objects with the
 * same key.
 *
 * @param {*} value The value to put.
 * @param {IDBKeyType=} opt_key The key to use. Cannot be used if the
 *     keyPath was specified for the object store. If the keyPath was not
 *     specified but autoIncrement was not enabled, it must be used.
 * @return {!goog.async.Deferred} The deferred put request.
 */
goog.db.ObjectStore.prototype.put = function(value, opt_key) {
  'use strict';
  return this.insert_(
      'put', 'putting into ' + this.getName() + ' with value', value, opt_key);
};


/**
 * Adds an object to the object store. Requires that there is no object with
 * the same key already present.
 *
 * @param {*} value The value to add.
 * @param {IDBKeyType=} opt_key The key to use. Cannot be used if the
 *     keyPath was specified for the object store. If the keyPath was not
 *     specified but autoIncrement was not enabled, it must be used.
 * @return {!goog.async.Deferred} The deferred add request.
 */
goog.db.ObjectStore.prototype.add = function(value, opt_key) {
  'use strict';
  return this.insert_(
      'add', 'adding into ' + this.getName() + ' with value ', value, opt_key);
};


/**
 * Removes an object from the store. No-op if there is no object present with
 * the given key.
 *
 * @param {IDBKeyType|!goog.db.KeyRange} keyOrRange The key or range to remove
 *     objects under.
 * @return {!goog.async.Deferred} The deferred remove request.
 */
goog.db.ObjectStore.prototype.remove = function(keyOrRange) {
  'use strict';
  const d = new goog.async.Deferred();
  let request;
  try {
    request = this.store_['delete'](
        keyOrRange instanceof goog.db.KeyRange ? keyOrRange.range() :
                                                 keyOrRange);
  } catch (err) {
    const msg = 'removing from ' + this.getName() + ' with key ' +
        goog.debug.deepExpose(keyOrRange);
    d.errback(goog.db.Error.fromException(err, msg));
    return d;
  }
  request.onsuccess = function(ev) {
    'use strict';
    d.callback();
  };
  const self = this;
  request.onerror = function(ev) {
    'use strict';
    const msg = 'removing from ' + self.getName() + ' with key ' +
        goog.debug.deepExpose(keyOrRange);
    d.errback(goog.db.Error.fromRequest(ev.target, msg));
  };
  return d;
};


/**
 * Gets an object from the store. If no object is present with that key
 * the result is `undefined`.
 *
 * @param {IDBKeyType} key The key to look up.
 * @return {!goog.async.Deferred} The deferred get request.
 */
goog.db.ObjectStore.prototype.get = function(key) {
  'use strict';
  const d = new goog.async.Deferred();
  let request;
  try {
    request = this.store_.get(key);
  } catch (err) {
    const msg = 'getting from ' + this.getName() + ' with key ' +
        goog.debug.deepExpose(key);
    d.errback(goog.db.Error.fromException(err, msg));
    return d;
  }
  request.onsuccess = function(ev) {
    'use strict';
    d.callback(ev.target.result);
  };
  const self = this;
  request.onerror = function(ev) {
    'use strict';
    const msg = 'getting from ' + self.getName() + ' with key ' +
        goog.debug.deepExpose(key);
    d.errback(goog.db.Error.fromRequest(ev.target, msg));
  };
  return d;
};


/**
 * Returns the values matching `opt_key` up to `opt_count`.
 *
 * If `obt_key` is a `KeyRange`, returns all keys in that range. If it is
 * `undefined`, returns all known keys.
 *
 * @param {!IDBKeyType|!goog.db.KeyRange=} opt_key Key or KeyRange to look up in
 *     the index.
 * @param {number=} opt_count The number records to return
 * @return {!goog.async.Deferred} A deferred array of objects that match the
 *     key.
 */
goog.db.ObjectStore.prototype.getAll = function(opt_key, opt_count) {
  'use strict';
  return this.getAllInternal_(
      'getAll', 'getting all from index ' + this.getName(), opt_key, opt_count);
};


/**
 * Returns the keys matching `opt_key` up to `opt_count`.
 *
 * If `obt_key` is a `KeyRange`, returns all keys in that range. If it is
 * `undefined`, returns all known keys.
 *
 * @param {!IDBKeyType|!goog.db.KeyRange=} opt_key Key or KeyRange to look up in
 *     the index.
 * @param {number=} opt_count The number records to return
 * @return {!goog.async.Deferred} A deferred array of keys for objects that
 *     match the key.
 */
goog.db.ObjectStore.prototype.getAllKeys = function(opt_key, opt_count) {
  'use strict';
  return this.getAllInternal_(
      'getAllKeys', 'getting all keys index ' + this.getName(), opt_key,
      opt_count);
};

/**
 * Helper function for native `getAll` and `getAllKeys` on `IDBObjectStore` that
 * takes in `IDBKeyRange` as params.
 *
 * Returns the result of the native method in a `Deferred` object.
 *
 * @param {string} fn Function name to call on the index to get the request.
 * @param {string} msg Message to give to the error.
 * @param {!IDBKeyType|!goog.db.KeyRange|undefined} keyOrRange
 *     Key or KeyRange to look up in the index.
 * @param {number|undefined} count The number records to return
 * @return {!goog.async.Deferred} The resulting deferred array of objects.
 * @private
 */
goog.db.ObjectStore.prototype.getAllInternal_ = function(
    fn, msg, keyOrRange, count) {
  'use strict';
  let nativeRange;
  if (keyOrRange === undefined) {
    nativeRange = undefined;
  } else if (keyOrRange instanceof goog.db.KeyRange) {
    nativeRange = keyOrRange.range();
  } else {
    nativeRange = goog.db.KeyRange.only(keyOrRange).range();
  }

  const d = new goog.async.Deferred();
  let request;
  try {
    request = this.store_[fn](nativeRange, count);
  } catch (err) {
    msg += ' for range ' +
        (nativeRange ? goog.debug.deepExpose(nativeRange) : '<all>');
    d.errback(goog.db.Error.fromException(err, msg));
    return d;
  }
  request.onsuccess = function() {
    'use strict';
    d.callback(request.result);
  };
  request.onerror = function(ev) {
    'use strict';
    msg += ' for range ' +
        (nativeRange ? goog.debug.deepExpose(nativeRange) : '<all>');
    d.errback(goog.db.Error.fromRequest(ev.target, msg));
  };
  return d;
};

/**
 * Opens a cursor over the specified key range. Returns a cursor object which is
 * able to iterate over the given range.
 *
 * Example usage:
 *
 * <code>
 *  var cursor = objectStore.openCursor(goog.db.Range.bound('a', 'c'));
 *
 *  var key = goog.events.listen(
 *      cursor, goog.db.Cursor.EventType.NEW_DATA, function() {
 *    // Do something with data.
 *    cursor.next();
 *  });
 *
 *  goog.events.listenOnce(
 *      cursor, goog.db.Cursor.EventType.COMPLETE, function() {
 *    // Clean up listener, and perform a finishing operation on the data.
 *    goog.events.unlistenByKey(key);
 *  });
 * </code>
 *
 * @param {!goog.db.KeyRange=} opt_range The key range. If undefined iterates
 *     over the whole object store.
 * @param {!goog.db.Cursor.Direction=} opt_direction The direction. If undefined
 *     moves in a forward direction with duplicates.
 * @return {!goog.db.Cursor} The cursor.
 * @throws {goog.db.Error} If there was a problem opening the cursor.
 */
goog.db.ObjectStore.prototype.openCursor = function(opt_range, opt_direction) {
  'use strict';
  return goog.db.Cursor.openCursor(this.store_, opt_range, opt_direction);
};


/**
 * Deletes all objects from the store.
 *
 * @return {!goog.async.Deferred} The deferred clear request.
 */
goog.db.ObjectStore.prototype.clear = function() {
  'use strict';
  const msg = 'clearing store ' + this.getName();
  const d = new goog.async.Deferred();
  let request;
  try {
    request = this.store_.clear();
  } catch (err) {
    d.errback(goog.db.Error.fromException(err, msg));
    return d;
  }
  request.onsuccess = function(ev) {
    'use strict';
    d.callback();
  };
  request.onerror = function(ev) {
    'use strict';
    d.errback(goog.db.Error.fromRequest(ev.target, msg));
  };
  return d;
};


/**
 * Creates an index in this object store. Can only be called inside a
 * {@link goog.db.UpgradeNeededCallback}.
 *
 * @param {string} name Name of the index to create.
 * @param {string|!Array<string>} keyPath Attribute or array of attributes to
 *     index on.
 * @param {!Object=} opt_parameters Optional parameters object. The only
 *     available option is unique, which defaults to false. If unique is true,
 *     the index will enforce that there is only ever one object in the object
 *     store for each unique value it indexes on.
 * @return {!goog.db.Index} The newly created, wrapped index.
 * @throws {goog.db.Error} In case of an error creating the index.
 */
goog.db.ObjectStore.prototype.createIndex = function(
    name, keyPath, opt_parameters) {
  'use strict';
  try {
    return new goog.db.Index(
        this.store_.createIndex(name, keyPath, opt_parameters));
  } catch (ex) {
    const msg = 'creating new index ' + name + ' with key path ' + keyPath;
    throw goog.db.Error.fromException(ex, msg);
  }
};


/**
 * Gets an index.
 *
 * @param {string} name Name of the index to fetch.
 * @return {!goog.db.Index} The requested wrapped index.
 * @throws {goog.db.Error} In case of an error getting the index.
 */
goog.db.ObjectStore.prototype.getIndex = function(name) {
  'use strict';
  try {
    return new goog.db.Index(this.store_.index(name));
  } catch (ex) {
    const msg = 'getting index ' + name;
    throw goog.db.Error.fromException(ex, msg);
  }
};


/**
 * Deletes an index from the object store. Can only be called inside a
 * {@link goog.db.UpgradeNeededCallback}.
 *
 * @param {string} name Name of the index to delete.
 * @throws {goog.db.Error} In case of an error deleting the index.
 */
goog.db.ObjectStore.prototype.deleteIndex = function(name) {
  'use strict';
  try {
    this.store_.deleteIndex(name);
  } catch (ex) {
    const msg = 'deleting index ' + name;
    throw goog.db.Error.fromException(ex, msg);
  }
};


/**
 * Gets number of records within a key range.
 *
 * @param {!goog.db.KeyRange=} opt_range The key range. If undefined, this will
 *     count all records in the object store.
 * @return {!goog.async.Deferred} The deferred number of records.
 */
goog.db.ObjectStore.prototype.count = function(opt_range) {
  'use strict';
  const d = new goog.async.Deferred();

  try {
    const range = opt_range ? opt_range.range() : null;
    const request = this.store_.count(range);
    request.onsuccess = function(ev) {
      'use strict';
      d.callback(ev.target.result);
    };
    const self = this;
    request.onerror = function(ev) {
      'use strict';
      d.errback(goog.db.Error.fromRequest(ev.target, self.getName()));
    };
  } catch (ex) {
    d.errback(goog.db.Error.fromException(ex, this.getName()));
  }

  return d;
};
