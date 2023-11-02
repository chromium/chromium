// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// namespace
export const storage = {};

/**
 * StorageAreaAsync is a wrapper for existing storage implementations to
 * include async/await compatible version of get/set.
 * @extends {StorageArea}
 */
class StorageAreaAsync {
  /**
   * @param {?StorageArea} storageArea chrome.storage.{local,sync} or null
   */
  constructor(storageArea) {
    this.storageArea_ = (storageArea) ? storageArea : this;

    /**
     * If the chrome.runtime.lastError should be checked.
     * @protected {boolean}
     */
    this.checkLastError = true;

    this.get = this.storageArea_.get.bind(this.storageArea_);
    this.set = this.storageArea_.set.bind(this.storageArea_);
    this.remove = this.storageArea_.remove.bind(this.storageArea_);
    this.clear = this.storageArea_.clear.bind(this.storageArea_);
  }

  /**
   * Convert the storage.{local,sync}.get method to return a Promise.
   * @param {string|!Array<string>} keys
   * @returns {!Promise<!Object<string, *>>}
   */
  async getAsync(keys) {
    return new Promise((resolve, reject) => {
      this.get(keys, (values) => {
        if (this.checkLastError && chrome && chrome.runtime &&
            chrome.runtime.lastError) {
          const keysString = keys && keys.join(', ');
          reject(`Failed to retrieve keys [${keysString}] from browser storage:
              ${chrome.runtime.lastError.message}`);
          return;
        }
        resolve(values);
      });
    });
  }

  /**
   * Convert the storage.{local,sync}.set method to return a Promise.
   * @param {!Object<string, *>} values
   * @returns {!Promise<void>}
   */
  async setAsync(values) {
    return new Promise((resolve, reject) => {
      this.set(values, () => {
        if (this.checkLastError && chrome && chrome.runtime &&
            chrome.runtime.lastError) {
          const keysString = values && Object.keys(values).join(', ');
          reject(`Failed to update browser storage keys
              [${keysString}] with supplied values:
              ${chrome.runtime.lastError.message}`);
          return;
        }
        resolve();
      });
    });
  }
}

/**
 * Propagates the changes to the storage keys.  It replicates the features of
 * chrome.storage.onChanged for the SWA.
 *
 * This does 3 things:
 * 1. Holds the onChanged event listeners/observes for the current window.
 * 2. Sends broadcast event to all windows.
 * 3. Listens to broadcast events and propagates to the listeners/observes in
 *    the current window.
 *
 * NOTE: This doesn't support the `oldValue` because it's simpler and the
 * current clients of `onChanged` don't need it.
 */
class StorageChangeTracker {
  constructor(storageNamespace) {
    /**
     * Listeners for the storage.onChanged.
     * @private {!Array<OnChangedListener>}
     * */
    this.observers_ = [];

    /**
     * Only used to send to the observers, because the chrome.storage.onChanged
     * sends the namespace in the event.
     * @private {string}
     */
    this.storageNamespace_ = storageNamespace;

    /**
     * Event to propagate changes to the localStorage to all windows listeners.
     */
    window.addEventListener('storage', this.onStorageEvent_.bind(this));
  }

  resetForTesting() {
    this.observers_ = [];
  }

  /**
   * Add a new listener to the onChanged event.
   * @param {function(!Object<string, !ValueChanged>, string)} callback
   */
  addListener(callback) {
    this.observers_.push(callback);
  }

  /**
   * Notifies all listeners of the key changes (triggers the onChanged event
   * listeners).
   * @param {!Object<string, *>} changedValues changed.
   * @param {string} namespace
   */
  keysChanged(changedValues, namespace) {
    /** @type {!Object<string, !ValueChanged>} */
    const changedKeys = {};

    for (const [k, v] of Object.entries(changedValues)) {
      // `oldValue` isn't necessary for the current use case.
      const key = /** @type {string} */ (k);
      changedKeys[key] = {newValue: v};
    }

    this.notifyLocally_(changedKeys, namespace);
  }

  /**
   * Process the `storage` event, propagates to all local listeners/observers.
   * @private
   */
  onStorageEvent_(event) {
    if (!event.key) {
      return;
    }
    /** @type {string} */
    const key = event.key;
    const newValue = /** @type {string} */ (event.newValue);
    const changedKeys = {};

    try {
      changedKeys[key] = {newValue: JSON.parse(newValue)};
    } catch (error) {
      console.warn(
          `Failed to JSON parse localStorage value from key: "${key}" ` +
              `returning the raw value.`,
          error);
      changedKeys[key] = {newValue};
    }
    this.notifyLocally_(changedKeys, this.storageNamespace_);
  }

  /**
   * Notifies the local listeners (in this window).
   * @param {!Object<string, ValueChanged>} keys
   * @param {string} namespace where the change occurred.
   * @private
   */
  notifyLocally_(keys, namespace) {
    for (const fn of this.observers_) {
      try {
        fn(keys, namespace);
      } catch (error) {
        console.error(`Error calling storage.onChanged listener: ${error}`);
      }
    }
  }
}

/**
 * StorageAreaSWAImpl enables the SWA version of Files app to continue using the
 * xfm.storage.* APIs by transparently switching them to use window.localStorage
 * instead of the chrome.storage APIs.
 */
class StorageAreaSWAImpl extends StorageAreaAsync {
  /**
   * @param {string} type
   */
  constructor(type) {
    super(/** storageArea */ null);

    this.checkLastError = false;

    /** @private {string} */
    this.type_ = type;
  }

  /** @override */
  get(keys, callback) {
    const inKeys = Array.isArray(keys) ? keys : [keys];
    const result = {};
    for (const key of inKeys) {
      result[key] = this.getValue_(key);
    }
    callback(result);
  }

  /**
   * Gets and parses the value from the storage.
   * @param {string} key
   * @private
   */
  getValue_(key) {
    const value = /** @type {string} */ (window.localStorage.getItem(key));
    try {
      return JSON.parse(value);
    } catch (error) {
      console.warn(
          `Failed to JSON parse localStorage value from key: "${key}" ` +
              `returning the raw value.`,
          error);
      return value;
    }
  }

  /** @override */
  set(items, opt_callback) {
    for (const key in items) {
      const value = JSON.stringify(items[key]);
      window.localStorage.setItem(key, value);
    }
    this.notifyChange_(Object.keys(items));
    if (opt_callback) {
      opt_callback();
    }
  }

  /** @override */
  remove(keys, callback) {
    const keyList = Array.isArray(keys) ? keys : [keys];
    for (const key of keyList) {
      window.localStorage.removeItem(key);
    }
    this.notifyChange_(keyList);
  }

  /** @override */
  clear(callback) {
    window.localStorage.clear();
    this.notifyChange_([]);
  }

  /**
   * Notifies the changes for `keys` to listeners of `onChanged`.
   * @param {!Array<string>} keys
   * @private
   */
  notifyChange_(keys) {
    const values = {};
    for (const k of keys) {
      values[k] = this.getValue_(k);
    }
    if (!storageChangeTracker) {
      console.error('Error xfm.storage requires the storageChangeTracker');
      return;
    }
    storageChangeTracker.keysChanged(values, this.type_);
  }
}

/**
 * NOTE: This in only available for legacy Files app and will be removed in the
 * future.
 * It's only used to allow FolderShortcuts to be migrated to prefs.
 * @type {!StorageAreaAsync}
 */
storage.sync;

/**
 * @type {!StorageAreaAsync}
 */
storage.local;

/**
 * @typedef {function(!Object<string, !ValueChanged>, string)}
 */
export let OnChangedListener;

/**
 * @typedef {{
 *    newValue: *,
 * }}
 */
export let ValueChanged;

/**
 * NOTE: Here we only expose the addListener() from the StorageChangeTracker.
 *
 * @type {{
 *   addListener: function(OnChangedListener),
 *   resetForTesting: function(),
 * }}
 */
storage.onChanged;

/** @private {?StorageChangeTracker} */
let storageChangeTracker = null;

storage.local = new StorageAreaSWAImpl('local');
storageChangeTracker = new StorageChangeTracker('local');
storage.onChanged = storageChangeTracker;
