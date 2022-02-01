// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Wrap calls.
// TODO(anastasi): Change interface to remove unneeded promises.

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

  /** @return {Promise<number>} */
  getDefaultDelta() {
    return new Promise(resolve => {
      resolve(this.defaultDelta_);
    });
  }

  /**
   * @param {number} delta
   * @return {Promise}
   */
  setDefaultDelta(delta) {
    if (!this.validDelta_(delta)) {
      delta = Storage.DEFAULT_DELTA;
    }
    this.defaultDelta_ = delta;
    return new Promise(
        resolve => this.store_(Storage.DELTA_TAG, delta, resolve));
  }

  /**
   * @param {string} site
   * @return {Promise<number>}
   */
  getSiteDelta(site) {
    return new Promise(resolve => {
      const delta = this.siteDeltas_[site];
      if (!this.validDelta_(delta)) {
        this.setSiteDelta(site, this.defaultDelta_);
        resolve(this.defaultDelta_);
        return;
      }
      resolve(delta);
    });
  }

  /**
   * @param {string} site
   * @param {number} delta
   * @return {Promise}
   */
  setSiteDelta(site, delta) {
    return new Promise(resolve => {
      if (!this.validDelta_(delta)) {
        delta = this.defaultDelta_;
      }
      this.siteDeltas_[site] = delta;
      this.store_(Storage.PER_SITE_DELTA_TAG, this.siteDeltas_, resolve);
    });
  }

  /** @return {Promise} */
  resetSiteDeltas() {
    this.siteDeltas_ = {};
    return new Promise(
        resolve => this.store_(Storage.PER_SITE_DELTA_TAG, {}, resolve));
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

  /** @return {Promise<number>} */
  getDefaultSeverity() {
    return new Promise(resolve => {
      resolve(this.defaultSeverity_);
    });
  }

  /**
   * @param {number} severity
   * @return {Promise}
   */
  setDefaultSeverity(severity) {
    if (!this.validSeverity_(severity)) {
      severity = Storage.DEFAULT_SEVERITY;
    }
    this.defaultSeverity_ = severity;
    return new Promise(
        resolve => this.store_(Storage.SEVERITY_TAG, severity, resolve));
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

  /** @return {Promise<!CvdType|Storage.INVALID_TYPE_PLACEHOLDER>} */
  getDefaultType() {
    return new Promise(resolve => {
      resolve(this.defaultType_);
    });
  }

  /**
   * @param {CvdType} type
   * @return {Promise}
   */
  setDefaultType(type) {
    if (!this.validType_(type)) {
      type = Storage.INVALID_TYPE_PLACEHOLDER;
    }
    this.defaultType_ = type;
    return new Promise(resolve => this.store_(Storage.TYPE_TAG, type, resolve));
  }

  // ======= Simulate setting =======

  /** @return {Promise<boolean>} */
  getDefaultSimulate() {
    return new Promise(resolve => {
      resolve(this.defaultSimulate_);
    });
  }

  /**
   * @param {boolean} simulate
   * @return {Promise}
   */
  setDefaultSimulate(simulate) {
    if (!this.validBoolean_(simulate)) {
      simulate = Storage.DEFAULT_SIMULATE;
    }
    this.defaultSimulate_ = simulate;
    return new Promise(
        resolve => this.store_(Storage.SIMULATE_TAG, simulate, resolve));
  }

  // ======= Enable setting =======

  /** @return {Promise<boolean>} */
  getDefaultEnable() {
    return new Promise(resolve => {
      resolve(this.defaultEnable_);
    });
  }

  /**
   * @param {boolean} enable
   * @return {Promise}
   */
  setDefaultEnable(enable) {
    if (!this.validBoolean_(enable)) {
      enable = Storage.DEFAULT_ENABLE;
    }
    this.defaultEnable_ = enable;
    return new Promise(
        resolve => this.store_(Storage.ENABLE_TAG, enable, resolve));
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
   * @param {function()} callback
   * @private
   */
  store_(key, val, callback) {
    const newVals = {};
    newVals[key] = val;
    chrome.storage.local.set(newVals, callback);
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
