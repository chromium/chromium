// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// namespace
export const storage = {};

/**
 * Class used to emit window.localStorage change events to event listeners.
 * This class does 3 things:
 *
 * 1. Holds the onChanged event listeners for the current window.
 * 2. Sends broadcast event to all windows.
 * 3. Listens to broadcast events and propagates to the listeners in
 *    the current window.
 *
 * NOTE: This doesn't support the `oldValue` because it's simpler and the
 * current clients of `onChanged` don't need it.
 */
class StorageChangeTracker {
  // @ts-ignore: error TS7006: Parameter 'storageNamespace' implicitly has an
  // 'any' type.
  constructor(storageNamespace) {
    /**
     * Storage onChanged event listeners for the current window.
     * @private @type {!Array<OnChangedListener>}
     * */
    // @ts-ignore: error TS7008: Member 'listeners_' implicitly has an 'any[]'
    // type.
    this.listeners_ = [];

    /**
     * Storage namespace argument added when calling listeners.
     * @private @type {string}
     */
    this.storageNamespace_ = storageNamespace;

    /**
     * Event to send local storage changes to all window listeners.
     */
    window.addEventListener('storage', this.onStorageEvent_.bind(this));
  }

  /**
   * Resets for testing: removes all listeners.
   */
  resetForTesting() {
    this.listeners_ = [];
  }

  /**
   * Adds an onChanged event listener for the current window.
   * @param {function(!Object<string, !ValueChanged>, string):void} callback
   */
  addListener(callback) {
    this.listeners_.push(callback);
  }

  /**
   * Notifies listeners_ of key value changes.
   * @param {!Object<string, *>} changedValues changed.
   */
  keysChanged(changedValues) {
    /** @type {!Object<string, !ValueChanged>} */
    const changedKeys = {};

    for (const [k, v] of Object.entries(changedValues)) {
      // `oldValue` isn't necessary for the current use case.
      const key = /** @type {string} */ (k);
      changedKeys[key] = {newValue: v};
    }

    this.notifyLocally_(changedKeys);
  }

  /**
   * Process localStorage `storage` event and notify listeners_.
   * @private
   */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onStorageEvent_(event) {
    if (!event.key) {
      return;
    }

    const key = /** @type {string} */ (event.key);
    const newValue = /** @type {string} */ (event.newValue);
    const changedKeys = {};

    try {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      changedKeys[key] = {newValue: JSON.parse(newValue)};
    } catch (error) {
      console.warn(
          `Failed to JSON parse localStorage value from key: "${key}" ` +
              `returning the raw value.`,
          error);
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      changedKeys[key] = {newValue};
    }

    // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
    // parameter of type '{ [x: string]: ValueChanged; }'.
    this.notifyLocally_(changedKeys);
  }

  /**
   * Notifies local (current window) listeners_ of key value changes.
   * @param {!Object<string, ValueChanged>} keys
   * @private
   */
  notifyLocally_(keys) {
    for (const listener of this.listeners_) {
      try {
        listener(keys, this.storageNamespace_);
      } catch (error) {
        console.error(`Error calling storage.onChanged listener: ${error}`);
      }
    }
  }
}

/**
 * StorageAreaImpl using window.localStorage as the storage area.
 */
class StorageAreaImpl {
  /**
   * @param {string} type
   */
  constructor(type) {
    /** @private @type {!StorageChangeTracker} */
    this.storageChangeTracker_ = new StorageChangeTracker(type);
  }

  /**
   * Gets values of |keys| and return them in the callback.
   * @param {string|!Array<string>} keys
   * @param {!function(!Object):void} callback
   */
  get(keys, callback) {
    const keyList = Array.isArray(keys) ? keys : [keys];
    const result = {};
    for (const key of keyList) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      result[key] = this.getValue_(key);
    }
    callback(result);
  }

  /**
   * Gets the value of |key| from local storage.
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

  /**
   * Async version of this.get() storage method.
   * @param {string|!Array<string>} keys
   * @returns {!Promise<!Object<string, *>>}
   */
  async getAsync(keys) {
    // @ts-ignore: error TS6133: 'reject' is declared but its value is never
    // read.
    return new Promise((resolve, reject) => {
      this.get(keys, (values) => {
        resolve(values);
      });
    });
  }

  /**
   * Stores items in local storage.
   * @param {!Object<string, *>} items The items to store.
   * @param {?function()=} opt_callback Optional callback to be called when
   *   the items have been stored.
   */
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

  /**
   * Async version of this.set() storage method.
   * @param {!Object<string, *>} items The items to store.
   * @returns {!Promise<void>}
   */
  async setAsync(items) {
    // @ts-ignore: error TS6133: 'reject' is declared but its value is never
    // read.
    return new Promise((resolve, reject) => {
      this.set(items, () => {
        resolve();
      });
    });
  }

  /**
   * Removes the given |keys| from local storage.
   * @param {string|!Array<string>} keys
   */
  remove(keys) {
    const keyList = Array.isArray(keys) ? keys : [keys];
    for (const key of keyList) {
      window.localStorage.removeItem(key);
    }
    this.notifyChange_(keyList);
  }

  /**
   * Clears local storage.
   */
  clear() {
    window.localStorage.clear();
    this.notifyChange_([]);
  }

  /**
   * Notifies key changes to storage change tracker listeners.
   * @param {!Array<string>} keys
   * @private
   */
  notifyChange_(keys) {
    const values = {};
    for (const k of keys) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      values[k] = this.getValue_(k);
    }

    this.getStorageChangeTracker_().keysChanged(values);
  }

  /**
   * Gets storage change tracker.
   * @returns {!StorageChangeTracker}
   * @private
   */
  getStorageChangeTracker_() {
    return this.storageChangeTracker_;
  }
}

/**
 * @type {!StorageAreaImpl}
 */
storage.local = new StorageAreaImpl('local');

/**
 * @typedef {function(!Object<string, !ValueChanged>, string):void}
 */
// @ts-ignore: error TS7005: Variable 'OnChangedListener' implicitly has an
// 'any' type.
export let OnChangedListener;

/**
 * @typedef {{
 *    newValue: *,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'ValueChanged' implicitly has an 'any'
// type.
export let ValueChanged;

/**
 * NOTE: Here we only expose StorageChangeTracker APIs addListener() and
 * resetForTesting().
 *
 * @type {{
 *   addListener: function(OnChangedListener):void,
 *   resetForTesting: function():void,
 * }}
 */
// @ts-ignore: error TS2341: Property 'getStorageChangeTracker_' is private and
// only accessible within class 'StorageAreaImpl'.
storage.onChanged = storage.local.getStorageChangeTracker_();
