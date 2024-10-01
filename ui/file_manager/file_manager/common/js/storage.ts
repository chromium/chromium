// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {debug} from './util.js';

export interface ChangedValue {
  newValue: any;
}

export type ChangedValues = Record<string, ChangedValue>;

export type OnChangedListener =
    (changedValues: ChangedValues, storageNamespace: string) => void;

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
  /** Storage onChanged event listeners for the current window. */
  private listeners_: OnChangedListener[] = [];

  /**
   * @param storageNamespace_ Storage namespace argument added when calling
   *     listeners.
   */
  constructor(private storageNamespace_: string) {
    /** Event to send local storage changes to all window listeners. */
    window.addEventListener('storage', this.onStorageEvent_.bind(this));
  }

  /** Resets for testing: removes all listeners. */
  resetForTesting() {
    this.listeners_ = [];
  }

  /** Adds an onChanged event listener for the current window. */
  addListener(callback: OnChangedListener) {
    this.listeners_.push(callback);
  }

  /** Notifies listeners of key value changes. */
  keysChanged(changedValues: Record<string, any>) {
    const changedKeys: ChangedValues = {};

    for (const [k, v] of Object.entries(changedValues)) {
      // `oldValue` isn't necessary for the current use case.
      changedKeys[k] = {newValue: v};
    }

    this.notifyLocally_(changedKeys);
  }

  /** Processes storage event and notifies listeners. */
  private onStorageEvent_(event: StorageEvent) {
    const {key, newValue} = event;
    if (key === null || newValue === null) {
      return;
    }

    const changedKeys: ChangedValues = {};

    try {
      changedKeys[key] = {newValue: JSON.parse(newValue)};
    } catch (error) {
      // This is expected when window.localStorage is used directly instead of
      // `local.storage` defined below.
      debug(
          `Cannot parse local storage value from key '${key}' as JSON`, error);
      changedKeys[key] = {newValue};
    }

    this.notifyLocally_(changedKeys);
  }

  /** Notifies local (current window) listeners of key value changes. */
  private notifyLocally_(keys: ChangedValues) {
    for (const listener of this.listeners_) {
      try {
        listener(keys, this.storageNamespace_);
      } catch (error) {
        console.error('Error calling storage.onChanged listener', error);
      }
    }
  }
}

/**
 * StorageAreaImpl using window.localStorage as the storage area.
 */
class StorageAreaImpl {
  readonly storageChangeTracker: StorageChangeTracker;

  constructor(type: string) {
    this.storageChangeTracker = new StorageChangeTracker(type);
  }

  /** Gets values of `keys` and returns them in the callback. */
  get(keys: string|string[], callback: (arg0: Record<string, any>) => void) {
    const keyList = Array.isArray(keys) ? keys : [keys];
    const result: Record<string, any> = {};
    for (const key of keyList) {
      result[key as string] = this.getValue_(key);
    }
    callback(result);
  }

  /** Gets the value of `key` from local storage. */
  private getValue_(key: string): any {
    const value = window.localStorage.getItem(key) as string;
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

  /** Async version of `this.get()`. */
  async getAsync(keys: string|string[]): Promise<Record<string, any>> {
    return new Promise(resolve => this.get(keys, resolve));
  }

  /**
   * Stores items in local storage.
   * @param items The items to store.
   * @param callback Callback to be called when the items have been stored.
   */
  set(items: Record<string, any>, callback?: () => void) {
    for (const key in items) {
      const value = JSON.stringify(items[key]);
      window.localStorage.setItem(key, value);
    }
    this.notifyChange_(Object.keys(items));
    callback?.();
  }

  /**
   * Async version of `this.set()`.
   * @param items The items to store.
   */
  async setAsync(items: Record<string, any>): Promise<void> {
    return new Promise(resolve => this.set(items, resolve));
  }

  /** Removes the given `keys` from local storage. */
  remove(keys: string|string[]) {
    const keyList = Array.isArray(keys) ? keys : [keys];
    for (const key of keyList) {
      window.localStorage.removeItem(key);
    }
    this.notifyChange_(keyList);
  }

  /** Clears local storage. */
  clear() {
    window.localStorage.clear();
    this.notifyChange_([]);
  }

  /** Notifies key changes to storage change tracker listeners. */
  private notifyChange_(keys: string[]) {
    const values: Record<string, any> = {};
    for (const k of keys) {
      values[k] = this.getValue_(k);
    }

    this.storageChangeTracker.keysChanged(values);
  }
}



export namespace storage {
  export const local = new StorageAreaImpl('local');
  export const onChanged: StorageChangeTracker = local.storageChangeTracker;
}
