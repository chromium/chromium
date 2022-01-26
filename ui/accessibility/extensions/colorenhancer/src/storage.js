// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Wrap calls.

class Storage {
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
      chrome.storage.local.get([Storage.DELTA_TAG], (result) => {
        let delta = result[Storage.DELTA_TAG];
        if (this.validDelta_(delta)) {
          resolve(delta);
          return;
        }
        delta = Storage.DEFAULT_DELTA;
        this.store_(Storage.DELTA_TAG, delta, () => resolve(delta));
      });
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
    return new Promise(
        resolve => this.store_(Storage.DELTA_TAG, delta, resolve));
  }

  /**
   * @param {string} site
   * @return {Promise<number>}
   */
  getSiteDelta(site) {
    return new Promise(resolve => {
      chrome.storage.local.get([Storage.PER_SITE_DELTA_TAG], (result) => {
        const siteDeltas = result [Storage.PER_SITE_DELTA_TAG] || {};
        const delta = siteDeltas[site];
        if (!this.validDelta_(delta)) {
          this.getDefaultDelta().then(resolve);
          return;
        }
        resolve(delta);
      });
    });
  }

  /**
   * @param {string} site
   * @param {number} delta
   * @return {Promise}
   */
  setSiteDelta(site, delta) {
    return new Promise(async resolve => {
      if (!this.validDelta_(delta)) {
        delta = await this.getDefaultDelta();
      }
      chrome.storage.local.get([Storage.PER_SITE_DELTA_TAG], (result) => {
        const siteDeltas = result[Storage.PER_SITE_DELTA_TAG] || {};
        siteDeltas[site] = delta;
        this.store_(Storage.PER_SITE_DELTA_TAG, siteDeltas, resolve);
      });
    });
  }

  /** @return {Promise} */
  resetSiteDeltas() {
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
      chrome.storage.local.get([Storage.SEVERITY_TAG], (result) => {
        let severity = result[Storage.SEVERITY_TAG];
        if (this.validSeverity_(severity)) {
          resolve(severity);
          return;
        }
        severity = Storage.DEFAULT_SEVERITY;
        this.store_(Storage.SEVERITY_TAG, severity, () => resolve(severity));
      });
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
    return type === 'PROTANOMALY' || type === 'DEUTERANOMALY' ||
        type === 'TRITANOMALY';
  }

  /** @return {Promise<string>} */
  getDefaultType() {
    return new Promise(resolve => {
      chrome.storage.local.get([Storage.TYPE_TAG], (result) => {
        const type = result[Storage.TYPE_TAG];
        if (this.validType_(type)) {
          resolve(type);
        } else {
          // TODO(anastasi): add appropriate error handling
          resolve(Storage.INVALID_TYPE_PLACEHOLDER);
        }
      });
    });
  }

  /**
   * @param {string} type
   * @return {Promise}
   */
  setDefaultType(type) {
    if (!this.validType_(type)) {
      type = Storage.INVALID_TYPE_PLACEHOLDER;
    }
    return new Promise(resolve => this.store_(Storage.TYPE_TAG, type, resolve));
  }

  // ======= Simulate setting =======

  /** @return {Promise<boolean>} */
  getDefaultSimulate() {
    return new Promise(resolve => {
      chrome.storage.local.get([Storage.SIMULATE_TAG], (result) => {
        let simulate = result[Storage.SIMULATE_TAG];

        if (this.validBoolean_(simulate)) {
          resolve(simulate);
          return;
        }
        simulate = Storage.DEFAULT_SIMULATE;
        this.store_(Storage.SIMULATE_TAG, simulate, () => resolve(simulate));
      });
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
    return new Promise(
        resolve => this.store_(Storage.SIMULATE_TAG, simulate, resolve));
  }

  // ======= Enable setting =======

  /** @return {Promise<boolean>} */
  getDefaultEnable() {
    return new Promise(resolve => {
      chrome.storage.local.get([Storage.ENABLE_TAG], (result) => {
        let enable = result[Storage.ENABLE_TAG];

        if (this.validBoolean_(enable)) {
          resolve(enable);
          return;
        }
        enable = Storage.DEFAULT_ENABLE;
        this.store_(Storage.ENABLE_TAG, enable, () => resolve(enable));
      });
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
