// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Storage {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.enabled_ = Storage.ENABLED.defaultValue;
    /** @private {number} */
    this.scheme_ = Storage.SCHEME.defaultValue;
    /** @private {!Object<string, number>} */
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
  /** @return {!KeyAction} */
  static get keyAction() { return Storage.instance.keyAction_; }
  /** @return {number} */
  static get scheme() { return Storage.instance.scheme_; }

  /**
   * @param {string} site
   * @return {number}
   */
  static getSiteScheme(site) {
    const scheme = Storage.instance.siteSchemes_[site];
    if (Storage.SCHEME.validate(scheme)) {
      return scheme;
    }
    return Storage.instance.scheme;
  }

  /** @param {boolean} enabled */
  static set enabled(enabled) {
    if (Storage.ENABLED.validate(enabled)) {
      Storage.instance.enabled_ = enabled;
    } else {
      Storage.ENABLED.reset();
    }
    Storage.instance.store_(Storage.ENABLED);
  }

  /** @param {number} scheme */
  static set scheme(scheme) {
    if (Storage.SCHEME.validate(scheme)) {
      Storage.instance.scheme_ = scheme;
    } else {
      Storage.SCHEME.reset();
    }
    Storage.instance.store_(Storage.SCHEME);
  }

  /**
   * @param {string} site
   * @param {number} scheme
   */
  static setSiteScheme(site, scheme) {
    if (Storage.SCHEME.validate(scheme)) {
      Storage.instance.siteSchemes_[site] = scheme;
    } else {
      Storage.instance.siteSchemes_[site] = Storage.instance.scheme;
    }
    Storage.instance.store_(Storage.SITE_SCHEMES);
  }

  static resetSiteSchemes() {
    Storage.instance.siteSchemes_ = Storage.SITE_SCHEMES.defaultValue;
    Storage.instance.store_(Storage.SITE_SCHEMES);
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
      const {newValue} = changes[value.key];

      if (value.validate(newValue)) {
        value.set(newValue);
      } else {
        value.reset();
      }
    }
  }

  /**
   * @param {!Storage.Value} value
   */
  store_(value) {
    const update = {};
    update[value.key] = value.get();
    chrome.storage.local.set(update);
  }

  // ======= Constants =======

  /** @const {number} */
  static MAX_SCHEME = 5;
  /** @constant {number} */
  static MIN_SCHEME = 0;

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
    defaultValue: true,
    validate: (enabled) => enabled === true || enabled === false,
    get: () => Storage.instance.enabled_,
    set: (enabled) => Storage.instance.enabled_ = enabled,
    reset: () => Storage.instance.setEnabled(Storage.ENABLED.defaultValue),
  };

  /** @const {!Storage.Value} */
  static SCHEME = {
    key: 'scheme',
    defaultValue: 3,
    validate: (scheme) =>
        scheme >= Storage.MIN_SCHEME && scheme <= Storage.MAX_SCHEME,
    get: () => Storage.instance.scheme_,
    set: (scheme) => Storage.instance.scheme_ = scheme,
    reset: () => Storage.instance.setScheme(Storage.SCHEME.defaultValue),
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
  };

  /** @const {!Array<!Storage.Value>} */
  static ALL_VALUES = [
      Storage.ENABLED, Storage.SCHEME, Storage.SITE_SCHEMES
  ];
}
