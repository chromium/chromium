// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Wrap calls, add JsDocs.

function validBoolean(b) {
  return b == true || b == false;
}

function store_(key, val, callback) {
  const newVals = {};
  newVals[key] = val;
  chrome.storage.local.set(newVals, callback);
}

// ======= Delta setting =======

/** @const {number} */ const DEFAULT_DELTA = 0.5;
/** @const {string} */ const LOCAL_STORAGE_TAG_DELTA = 'cvd_delta';
/** @const {string} */ const LOCAL_STORAGE_TAG_SITE_DELTA = 'cvd_site_delta';

function validDelta(delta) {
  return delta >= 0 && delta <= 1;
}

function getDefaultDelta() {
  return new Promise(resolve => {
    chrome.storage.local.get([LOCAL_STORAGE_TAG_DELTA], (result) => {
      let delta = result[LOCAL_STORAGE_TAG_DELTA];
      if (validDelta(delta)) {
        resolve(delta);
        return;
      }
      delta = DEFAULT_DELTA;
      store_(LOCAL_STORAGE_TAG_DELTA, delta, () => resolve(delta));
    });
  });
}

function setDefaultDelta(delta) {
  if (!validDelta(delta)) {
    delta = DEFAULT_DELTA;
  }
  return new Promise(
      resolve => store_(LOCAL_STORAGE_TAG_DELTA, delta, resolve));
}

function getSiteDelta(site) {
  return new Promise(resolve => {
    chrome.storage.local.get([LOCAL_STORAGE_TAG_SITE_DELTA], (result) => {
      let delta;
      try {
        const siteDeltas = result[LOCAL_STORAGE_TAG_SITE_DELTA] || {};
        delta = siteDeltas[site];
        if (!validDelta(delta)) {
          getDefaultDelta().then(resolve);
          return;
        }
      } catch (e) {
        getDefaultDelta().then(resolve);
        return;
      }
      resolve(delta);
    });
  });
}

function setSiteDelta(site, delta) {
  return new Promise(async resolve => {
    if (!validDelta(delta)) {
      delta = await getDefaultDelta();
    }
    chrome.storage.local.get([LOCAL_STORAGE_TAG_SITE_DELTA], (result) => {
      let siteDeltas = {};
      try {
        siteDeltas = result[LOCAL_STORAGE_TAG_SITE_DELTA] || {};
      } catch (e) {
        siteDeltas = {};
      }
      siteDeltas[site] = delta;
      store_(LOCAL_STORAGE_TAG_SITE_DELTA, siteDeltas, resolve);
    });
  });
}

function resetSiteDeltas() {
  return new Promise(
      resolve => store_(LOCAL_STORAGE_TAG_SITE_DELTA, {}, resolve));
}

// ======= Severity setting =======

/** @const {number} */ const DEFAULT_SEVERITY = 1.0;
/** @const {string} */ const LOCAL_STORAGE_TAG_SEVERITY = 'cvd_severity';


function validSeverity(severity) {
  return severity >= 0 && severity <= 1;
}

function getDefaultSeverity() {
  return new Promise(resolve => {
    chrome.storage.local.get([LOCAL_STORAGE_TAG_SEVERITY], (result) => {
      let severity = result[LOCAL_STORAGE_TAG_SEVERITY];
      if (validSeverity(severity)) {
        resolve(severity);
        return;
      }
      severity = DEFAULT_SEVERITY;
      store_(LOCAL_STORAGE_TAG_SEVERITY, severity, () => resolve(severity));
    });
  });
}

function setDefaultSeverity(severity) {
  if (!validSeverity(severity)) {
    severity = DEFAULT_SEVERITY;
  }
  return new Promise(
      resolve => store_(LOCAL_STORAGE_TAG_SEVERITY, severity, resolve));
}

// ======= Type setting =======

/** @const {string} */ const INVALID_TYPE_PLACEHOLDER = '';
/** @const {string} */ const LOCAL_STORAGE_TAG_TYPE = 'cvd_type';

function validType(type) {
  return type === 'PROTANOMALY' ||
      type === 'DEUTERANOMALY' ||
      type === 'TRITANOMALY';
}

function getDefaultType() {
  return new Promise(resolve => {
    chrome.storage.local.get([LOCAL_STORAGE_TAG_TYPE], (result) => {
      const type = result[LOCAL_STORAGE_TAG_TYPE];
      if (validType(type)) {
        resolve(type);
      } else {
        // TODO(anastasi): add appropriate error handling
        resolve();
      }
    });
  });
}

function setDefaultType(type) {
  if (!validType(type)) {
    type = INVALID_TYPE_PLACEHOLDER;
  }
  return new Promise(
      resolve => store_(LOCAL_STORAGE_TAG_TYPE, type, resolve));
}

// ======= Simulate setting =======

/** @const {boolean} */ const DEFAULT_SIMULATE = false;
/** @const {string} */ const LOCAL_STORAGE_TAG_SIMULATE = 'cvd_simulate';

function getDefaultSimulate() {
  return new Promise(resolve => {
    chrome.storage.local.get([LOCAL_STORAGE_TAG_SIMULATE], (result) => {
      let simulate = result[LOCAL_STORAGE_TAG_SIMULATE];

      if (validBoolean(simulate)) {
        resolve(simulate);
        return;
      }
      simulate = DEFAULT_SIMULATE;
      store_(LOCAL_STORAGE_TAG_SIMULATE, simulate, () => resolve(simulate));
    });
  });
}

function setDefaultSimulate(simulate) {
  if (!validBoolean(simulate)) {
    simulate = DEFAULT_SIMULATE;
  }
  return new Promise(
      resolve => store_(LOCAL_STORAGE_TAG_SIMULATE, simulate, resolve));
}

// ======= Enable setting =======

/** @const {boolean} */ const DEFAULT_ENABLE = false;
/** @const {string} */ const LOCAL_STORAGE_TAG_ENABLE = 'cvd_enable';

function getDefaultEnable() {
  return new Promise(resolve => {
    chrome.storage.local.get([LOCAL_STORAGE_TAG_ENABLE], (result) => {
      let enable = result[LOCAL_STORAGE_TAG_ENABLE];

      if (validBoolean(enable)) {
        resolve(enable);
        return;
      }
      enable = DEFAULT_ENABLE;
      store_(LOCAL_STORAGE_TAG_ENABLE, enable, () => resolve(enable));
    });
  });
}

function setDefaultEnable(enable) {
  if (!validBoolean(enable)) {
    enable = DEFAULT_ENABLE;
  }
  return new Promise(
      resolve => store_(LOCAL_STORAGE_TAG_ENABLE, enable, resolve));
}
