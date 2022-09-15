// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// namespace
export const power = {};

if ('wakeLock' in navigator) {
  power.lock_ = null;
  /** @type {function(string):!Promise<boolean>} */
  power.requestKeepAwake = async (type) => {
    try {
      if (power.lock_ !== null) {
        console.warn('Next wakeLock requested before wakeLock release called');
      }
      // type == 'system' is not supported, we always use screen.
      power.lock_ = await navigator.wakeLock.request('screen');
      power.lock_.addEventListener('release', () => {
        power.lock_ = null;
      });
    } catch (err) {
      console.warn('failed to acquire wakeLock: ', err);
    }
    return power.lock_ !== null;
  };
  /** @type {function():void} */
  power.releaseKeepAwake = () => {
    if (power.lock_) {
      power.lock_.release();
    }
  };
} else {
  power.callCount = 0;
  power.awake = false;
  power.requestKeepAwake = async (type) => {
    ++power.callCount;
    power.awake = true;
    return true;
  };
  power.releaseKeepAwake = () => {
    ++power.callCount;
    power.awake = false;
  };
}
