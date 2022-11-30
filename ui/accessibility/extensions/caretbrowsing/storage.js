// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const FlourishType = {
  ANIMATE: 'anim',
  FLASH: 'flash',
  NONE: 'none',
};

/**
 * Class to handle interactions with the chrome.storage API and values that are
 * stored that way.
 */
class Storage {
  /**
   * @param {function()=} opt_callbackForTesting
   * @private
   */
  constructor(opt_callbackForTesting) {
    /** @private {boolean} */
    this.enabled_ = Storage.ENABLED.defaultValue;
    /** @private {!FlourishType} */
    this.onEnable_ = Storage.ON_ENABLE.defaultValue;
    /** @private {!FlourishType} */
    this.onJump_ = Storage.ON_JUMP.defaultValue;

    this.init_(opt_callbackForTesting);
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

  /** @param {boolean} newValue */
  static set enabled(newValue) {
    Storage.instance.setOrResetValue_(Storage.ENABLED, newValue);
    Storage.instance.store_(Storage.ENABLED);
  }

  /** @param {!FlourishType} newBehavior */
  static set onEnable(newBehavior) {
    Storage.instance.setOrResetValue_(Storage.ON_ENABLE, newBehavior);
    Storage.instance.store_(Storage.ON_ENABLE);
  }

  /** @param {!FlourishType} newBehavior */
  static set onJump(newBehavior) {
    Storage.instance.setOrResetValue_(Storage.ON_JUMP, newBehavior);
    Storage.instance.store_(Storage.ON_JUMP);
  }

  // ======= Private Methods =======

  /**
   * @param {!Storage.Value} container
   * @param {*} newValue
   * @private
   */
  setOrResetValue_(container, newValue) {
    if (newValue === container.get()) {
      return;
    }

    if (container.validate(newValue)) {
      container.set(newValue);
    } else {
      container.reset();
    }

    container.listeners.forEach(listener => listener(newValue));
  }

  /**
   * @param {function()=} opt_callback
   * @private
   */
  init_(opt_callback) {
    chrome.storage.onChanged.addListener(this.onChange_);
    chrome.storage.local.get(null /* all values */, (results) => {
      const storedValues = Storage.ALL_VALUES.filter(v => results[v.key]);
      for (const value of storedValues) {
        this.setOrResetValue_(value, results[value.key]);
      }
      opt_callback ? opt_callback() : undefined;
    });
  }

  /**
   * @param {!Object<string, chrome.storage.StorageChange>} changes
   * @private
   */
  onChange_(changes) {
    const changedValues = Storage.ALL_VALUES.filter(v => changes[v.key]);
    for (const value of changedValues) {
      Storage.instance.setOrResetValue_(value, changes[value.key].newValue);
    }
  }

  /**
   * @param {!Storage.Value} value
   * @private
   */
  store_(value) {
    chrome.storage.local.set({ [value.key]: value.get() });
  }

  // ======= Stored Values =======

  /**
   * @typedef {{
   *     key: string,
   *     defaultValue: *,
   *     validate: function(*): boolean,
   *     get: function: *,
   *     set: function(*),
   *     reset: function(),
   *     listeners: !Array<function(*)>
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
    reset: () => Storage.instance.enabled_ = Storage.ENABLED.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static ON_ENABLE = {
    key: 'onenable',
    defaultValue: FlourishType.ANIMATE,
    validate: (onEnable) => Object.values(FlourishType).includes(onEnable),
    get: () => Storage.instance.onEnable_,
    set: (onEnable) => Storage.instance.onEnable_ = onEnable,
    reset: () => Storage.instance.onEnable_ = Storage.ON_ENABLE.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static ON_JUMP = {
    key: 'onjump',
    defaultValue: FlourishType.FLASH,
    validate: (onJump) => Object.values(FlourishType).includes(onJump),
    get: () => Storage.instance.onJump_,
    set: (onJump) => Storage.instance.onJump_ = onJump,
    reset: () => Storage.instance.onJump_ = Storage.ON_JUMP.defaultValue,
    listeners: [],
  };

  /** @const {!Array<!Storage.Value>} */
  static ALL_VALUES = [
      Storage.ENABLED, Storage.ON_ENABLE, Storage.ON_JUMP,
  ];
}
