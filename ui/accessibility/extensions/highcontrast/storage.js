// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {number} */
const SchemeType = {
  NORMAL: 0,
  INCREASED_CONTRAST: 1,
  GRAYSCALE: 2,
  INVERTED_COLOR: 3,
  INVERTED_GRAYSCALE: 4,
  YELLOW_ON_BLACK: 5,
};

/**
 * Class to handle interactions with the chrome.storage API and values that are
 * stored there.
 */
class Storage {
  /**
   * @param {function()=} opt_callbackForTesting
   * @private
   */
  constructor(opt_callbackForTesting) {
    /** @private {boolean} */
    this.enabled_ = Storage.ENABLED.defaultValue;
    /** @private {!SchemeType} */
    this.baseScheme_ = Storage.SCHEME.defaultValue;
    /** @private {!Object<string, !SchemeType>} */
    this.siteSchemes_ = Storage.SITE_SCHEMES.defaultValue;

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
  /** @return {!SchemeType} */
  static get baseScheme() { return Storage.instance.baseScheme_; }

  /**
   * @param {string} site
   * @return {!SchemeType}
   */
  static getSiteScheme(site) {
    const scheme = Storage.instance.siteSchemes_[site];
    if (Storage.SCHEME.validate(scheme)) {
      return scheme;
    }
    return Storage.baseScheme;
  }

  /** @param {boolean} newValue */
  static set enabled(newValue) {
    Storage.instance.setOrResetValue_(Storage.ENABLED, newValue);
    Storage.instance.store_(Storage.ENABLED);
  }

  /** @param {!SchemeType} newScheme */
  static set baseScheme(newScheme) {
    Storage.instance.setOrResetValue_(Storage.SCHEME, newScheme);
    Storage.instance.store_(Storage.SCHEME);
  }

  /**
   * @param {string} site
   * @param {!SchemeType} scheme
   */
  static setSiteScheme(site, scheme) {
    if (Storage.SCHEME.validate(scheme)) {
      Storage.instance.siteSchemes_[site] = scheme;
    } else {
      Storage.instance.siteSchemes_[site] = Storage.baseScheme;
    }
    Storage.instance.store_(Storage.SITE_SCHEMES);
  }

  static resetSiteSchemes() {
    Storage.instance.siteSchemes_ = Storage.SITE_SCHEMES.defaultValue;
    Storage.instance.store_(Storage.SITE_SCHEMES);
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
      const storedData = Storage.ALL_VALUES.filter(v => results[v.key]);
      for (const data of storedData) {
        this.setOrResetValue_(data, results[data.key]);
      }
      opt_callback ? opt_callback() : undefined;
    });
  }

  /**
   * @param {!Object<string, chrome.storage.StorageChange>} changes
   * @private
   */
  onChange_(changes) {
    const changedData = Storage.ALL_VALUES.filter(v => changes[v.key]);
    for (const data of changedData) {
      Storage.instance.setOrResetValue_(data, changes[data.key].newValue);
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
    defaultValue: true,
    validate: (enabled) => enabled === true || enabled === false,
    get: () => Storage.instance.enabled_,
    set: (enabled) => Storage.instance.enabled_ = enabled,
    reset: () => Storage.instance.enabled_ = Storage.ENABLED.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static SCHEME = {
    key: 'scheme',
    defaultValue: SchemeType.INVERTED_COLOR,
    validate: (scheme) => Object.values(SchemeType).includes(scheme),
    get: () => Storage.instance.baseScheme_,
    set: (scheme) => Storage.instance.baseScheme_ = scheme,
    reset: () => Storage.instance.baseScheme_ = Storage.SCHEME.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static SITE_SCHEMES = {
    key: 'siteschemes',
    defaultValue: {},
    validate: (siteSchemes) => typeof (siteSchemes) === 'object',
    get: () => Storage.instance.siteSchemes_,
    set: (siteSchemes) => {
      for (const site of Object.keys(siteSchemes)) {
        if (Storage.SCHEME.validate(siteSchemes[site])) {
          Storage.instance.siteSchemes_[site] = siteSchemes[site];
        }
      }
    },
    reset: () => {} /** Do nothing */,
    listeners: [],
  };

  /** @const {!Array<!Storage.Value>} */
  static ALL_VALUES = [
      Storage.ENABLED, Storage.SCHEME, Storage.SITE_SCHEMES
  ];
}
