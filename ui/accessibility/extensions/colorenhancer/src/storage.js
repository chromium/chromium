// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Wrap calls.

/** @typedef {CvdType|number|boolean} */
let StoredValue;
/** @typedef {{newValue: StoredValue, oldValue: StoredValue}} */
let Change;

class Storage {
  constructor() {
    /** @private {number} */
    this.defaultDelta_ = Storage.DEFAULT_DELTA;

    /** @private {!Object<string, number>} */
    this.siteDeltas_ = {};

    /** @private {number} */
    this.defaultSeverity_ = Storage.DEFAULT_SEVERITY;

    /** @private {!CvdType|Storage.INVALID_TYPE_PLACEHOLDER} */
    this.defaultType_ = Storage.INVALID_TYPE_PLACEHOLDER;

    /** @private {boolean} */
    this.defaultSimulate_ = Storage.DEFAULT_SIMULATE;

    /** @private {boolean} */
    this.defaultEnable_ = Storage.DEFAULT_ENABLE;

    this.init_();
  }

  /** @private */
  init_() {
    chrome.storage.onChanged.addListener(
        this.onStorageChanged_.bind(this, (change) => change.newValue));
    chrome.storage.local.get(
        null /** all items */,
        this.onStorageChanged_.bind(this, (newVal) => newVal));
  }

  /**
   * Updates the cached values.
   * @param {(function(Change): StoredValue) |
   *    (function(StoredValue): StoredValue)} getValueFromAPIResult Gets the
   *      new value from the result indexed at a key. When called by
   *      storage.local.get(), this returns exactly what it is given. When
   *      called by storage.onChanged, this extracts newValue (instead of
   *      oldValue).
   * @param {Object<string, Change>|Object<string, StoredValue>} changes The
   *      updates from the chrome.storage API.
   * @private
   */
  onStorageChanged_(getValueFromAPIResult, changes) {
    if (changes[Storage.DELTA_TAG]) {
      const newVal = getValueFromAPIResult(changes[Storage.DELTA_TAG]);
      if (this.validDelta_(newVal)) {
        this.defaultDelta_ = newVal;
      } else {
        this.defaultDelta_ = Storage.DEFAULT_DELTA;
      }
    }

    if (changes[Storage.PER_SITE_DELTA_TAG]) {
      const newVal = getValueFromAPIResult(changes[Storage.PER_SITE_DELTA_TAG]);
      if (typeof (newVal) === 'object') {
        for (const site of Object.keys(newVal)) {
          if (!this.validDelta_(newVal[site])) {
            newVal[site] = this.defaultDelta_;
          }
        }
        this.siteDeltas_ = newVal;
      } else {
        this.siteDeltas_ = {};
      }
    }

    if (changes[Storage.SEVERITY_TAG]) {
      const newVal = getValueFromAPIResult(changes[Storage.SEVERITY_TAG]);
      if (this.validSeverity_(newVal)) {
        this.defaultSeverity_ = newVal;
      } else {
        this.defaultSeverity_ = Storage.DEFAULT_SEVERITY;
      }
    }

    if (changes[Storage.TYPE_TAG]) {
      const newVal = getValueFromAPIResult(changes[Storage.TYPE_TAG]);
      if (this.validType_(newVal)) {
        this.defaultType_ = newVal;
      }
    }

    if (changes[Storage.SIMULATE_TAG]) {
      const newVal = getValueFromAPIResult(changes[Storage.SIMULATE_TAG]);
      if (this.validBoolean_(newVal)) {
        this.defaultSimulate_ = newVal;
      } else {
        this.defaultSimulate_ = Storage.DEFAULT_SIMULATE;
      }
    }

    if (changes[Storage.ENABLE_TAG]) {
      const newVal = getValueFromAPIResult(changes[Storage.ENABLE_TAG]);
      if (this.validBoolean_(newVal)) {
        this.defaultEnable_ = newVal;
      } else {
        this.defaultEnable_ = Storage.DEFAULT_ENABLE;
      }
    }
  }

  // ======= Delta setting =======

  /**
   * @param {number} delta
   * @return {boolean}
   * @private
   */
  validDelta_(delta) {
    return delta >= 0 && delta <= 1;
  }

  /** @return {number} */
  getDefaultDelta() {
    return this.defaultDelta_;
  }

  /**
   * @param {number} delta
   */
  setDefaultDelta(delta) {
    if (!this.validDelta_(delta)) {
      delta = Storage.DEFAULT_DELTA;
    }
    this.defaultDelta_ = delta;
    this.store_(Storage.DELTA_TAG, delta);
  }

  /**
   * @param {string} site
   * @return {number}
   */
  getSiteDelta(site) {
    const delta = this.siteDeltas_[site];
    if (!this.validDelta_(delta)) {
      this.setSiteDelta(site, this.defaultDelta_);
      return this.defaultDelta_;
    }
    return delta;
  }

  /**
   * @param {string} site
   * @param {number} delta
   */
  setSiteDelta(site, delta) {
    if (!this.validDelta_(delta)) {
      delta = this.defaultDelta_;
    }
    this.siteDeltas_[site] = delta;
    this.store_(Storage.PER_SITE_DELTA_TAG, this.siteDeltas_);
  }

  resetSiteDeltas() {
    this.siteDeltas_ = {};
    this.store_(Storage.PER_SITE_DELTA_TAG, {});
  }

  // ======= Severity setting =======

  /**
   * @param {number} severity
   * @return {boolean}
   * @private
   */
  validSeverity_(severity) {
    return severity >= 0 && severity <= 1;
  }

  /** @return {number} */
  getDefaultSeverity() {
    return this.defaultSeverity_;
  }

  /**
   * @param {number} severity
   */
  setDefaultSeverity(severity) {
    if (!this.validSeverity_(severity)) {
      severity = Storage.DEFAULT_SEVERITY;
    }
    this.defaultSeverity_ = severity;
    this.store_(Storage.SEVERITY_TAG, severity);
  }

  // ======= Type setting =======

  /**
   * @param {string} type
   * @return {boolean}
   * @private
   */
  validType_(type) {
    return Object.values(CvdType).includes(type);
  }

  /** @return {!CvdType|Storage.INVALID_TYPE_PLACEHOLDER} */
  getDefaultType() {
    return this.defaultType_;
  }

  /**
   * @param {CvdType} type
   */
  setDefaultType(type) {
    if (!this.validType_(type)) {
      type = Storage.INVALID_TYPE_PLACEHOLDER;
    }
    this.defaultType_ = type;
    this.store_(Storage.TYPE_TAG, type);
  }

  // ======= Simulate setting =======

  /** @return {boolean} */
  getDefaultSimulate() {
    return this.defaultSimulate_;
  }

  /**
   * @param {boolean} simulate
   */
  setDefaultSimulate(simulate) {
    if (!this.validBoolean_(simulate)) {
      simulate = Storage.DEFAULT_SIMULATE;
    }
    this.defaultSimulate_ = simulate;
    this.store_(Storage.SIMULATE_TAG, simulate);
  }

  // ======= Enable setting =======

  /** @return {boolean} */
  getDefaultEnable() {
    return this.defaultEnable_;
  }

  /**
   * @param {boolean} enable
   */
  setDefaultEnable(enable) {
    if (!this.validBoolean_(enable)) {
      enable = Storage.DEFAULT_ENABLE;
    }
    this.defaultEnable_ = enable;
    this.store_(Storage.ENABLE_TAG, enable);
  }

  // ======= Helper functions ========

  /**
   * @return {boolean}
   * @private
   */
  validBoolean_(b) {
    return b == true || b == false;
  }

  /**
   * @param {*} key
   * @param {*} val
   * @param {function()|undefined} opt_callback
   * @private
   */
  store_(key, val, opt_callback) {
    const newVals = {};
    newVals[key] = val;
    chrome.storage.local.set(newVals, opt_callback);
  }
}

/** @const {number} */
Storage.DEFAULT_DELTA = 0.5;
/** @const {string} */
Storage.DELTA_TAG = 'cvd_delta';
/** @const {string} */
Storage.PER_SITE_DELTA_TAG = 'cvd_site_delta';

/** @const {number} */
Storage.DEFAULT_SEVERITY = 1.0;
/** @const {string} */
Storage.SEVERITY_TAG = 'cvd_severity';

/** @const {string} */
Storage.INVALID_TYPE_PLACEHOLDER = '';
/** @const {string} */
Storage.TYPE_TAG = 'cvd_type';

/** @const {boolean} */
Storage.DEFAULT_SIMULATE = false;
/** @const {string} */
Storage.SIMULATE_TAG = 'cvd_simulate';

/** @const {boolean} */
Storage.DEFAULT_ENABLE = false;
/** @const {string} */
Storage.ENABLE_TAG = 'cvd_enable';
