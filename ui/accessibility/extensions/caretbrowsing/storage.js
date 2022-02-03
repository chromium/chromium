// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const FlourishType = {
  ANIMATE: 'anim',
  FLASH: 'flash',
  NONE: 'none',
};

/**
 * Class to handle both storing values using the chrome.storage API, and
 * fetching/caching values that have been stored that way.
 */
class Storage {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.enabled_ = Storage.ENABLED.defaultValue;
    /** @private {!FlourishType} */
    this.onEnable_ = Storage.ON_ENABLE.defaultValue;
    /** @private {!FlourishType} */
    this.onJump_ = Storage.ON_JUMP.defaultValue;

    this.init_();
  }

  // ======= Public Methods =======

  static initialize() {
    if (!Storage.instance) {
      Storage.instance = new Storage();
    }
  }

  /** @return {boolean} */
  static get enabled() { return Storage.instance.enabled_; }
  /** @return {!FlourishType} */
  static get onEnable() { return Storage.instance.onEnable_; }
  /** @return {!FlourishType} */
  static get onJump() { return Storage.instance.onJump_; }

  /** @param {boolean} enabled */
  static set enabled(enabled) {
    Storage.instance.setValue_(Storage.ENABLED, enabled);
  }

  /** @param {!FlourishType} onEnable */
  static set onEnable(onEnable) {
    Storage.instance.setValue_(Storage.ON_ENABLE, onEnable);
  }

  /** @param {!FlourishType} onJump */
  static set onJump(onJump) {
    Storage.instance.setValue_(Storage.ON_JUMP, onJump);
  }

  /**
   * @param {!Storage.Value} storage
   * @param {*} newValue
   * @private
   */
  setValue_(storage, newValue) {
    if (newValue === storage.get()) {
      return;
    }

    if (storage.validate(newValue)) {
      storage.set(newValue);
    } else {
      storage.reset();
    }
    this.store_(storage);
  }

  // ======= Private Methods =======

  /** @private */
  init_() {
    chrome.storage.onChanged.addListener(this.onChange_);
    chrome.storage.local.get(null /* all values */, (results) => {
      for (const value of Storage.ALL_VALUES) {
        const newValue = results[value.key];
        if (!newValue) {
          continue;
        }

        if (value.validate(newValue)) {
          value.set(newValue);
        } else {
          value.reset();
        }
      }
    });
  }

  /**
   * @param {!Object<string, chrome.storage.StorageChange>} changes
   * @private
   */
  onChange_(changes) {
    for (const value of Storage.ALL_VALUES) {
      if (!changes[value.key]) {
        continue;
      }

      const newValue = changes[value.key].newValue;
      if (value.validate(newValue)) {
        value.set(newValue);
      } else {
        value.reset();
      }
    }
  }

  /**
   * @param {!Storage.Value} value
   * @private
   */
  store_(value) {
    const update = {};
    update[value.key] = value.get();
    chrome.storage.local.set(update);
  }

  // ======= Stored Values =======

  /**
   * @typedef {{
   *     key: string,
   *     defaultValue: *,
   *     validate: function(*): boolean,
   *     get: function: *,
   *     set: function(*),
   *     reset: function()
   * }}
   */
  static Value;

  /** @const {!Storage.Value} */
  static ENABLED = {
    key: 'enabled',
    defaultValue: false,
    validate: (enabled) => enabled === true || enabled === false,
    get: () => Storage.instance.enabled_,
    set: (enabled) => Storage.instance.enabled_ = enabled,
    reset: () => Storage.instance.setEnabled(Storage.ENABLED.defaultValue),
  };

  /** @const {!Storage.Value} */
  static ON_ENABLE = {
    key: 'onenable',
    defaultValue: FlourishType.ANIMATE,
    validate: (onEnable) => Object.values(FlourishType).includes(onEnable),
    get: () => Storage.instance.onEnable_,
    set: (onEnable) => Storage.instance.onEnable_ = onEnable,
    reset: () => Storage.instance.setKeyAction(Storage.ON_ENABLE.defaultValue),
  };

  /** @const {!Storage.Value} */
  static ON_JUMP = {
    key: 'onjump',
    defaultValue: FlourishType.FLASH,
    validate: (onJump) => Object.values(FlourishType).includes(onJump),
    get: () => Storage.instance.onJump_,
    set: (onJump) => Storage.instance.onJump_ = onJump,
    reset: () => Storage.instance.setKeyAction(Storage.ON_JUMP.defaultValue),
  };

  /** @const {!Array<!Storage.Value>} */
  static ALL_VALUES = [
      Storage.ENABLED, Storage.ON_ENABLE, Storage.ON_JUMP,
  ];
}
