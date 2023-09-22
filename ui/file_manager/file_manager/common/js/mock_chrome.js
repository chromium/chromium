// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Installs a mock object to replace window.chrome in a unit test.
 * @param {!Object} mockChrome
 */
export function installMockChrome(mockChrome) {
  /** @suppress {const|checkTypes} */
  window.chrome = window.chrome || {};
  mockChrome.metricsPrivate = mockChrome.metricsPrivate || new MockMetrics();

  const chrome = window.chrome;
  for (const [key, value] of Object.entries(mockChrome)) {
    const target = chrome[key] || value;
    Object.assign(target, value);
    chrome[key] = target;
  }
}

/**
 * Mocks chrome.commandLinePrivate.
 */
export class MockCommandLinePrivate {
  constructor() {
    this.flags_ = {};
    if (!chrome) {
      installMockChrome({});
    }

    if (!chrome.commandLinePrivate) {
      /** @suppress {checkTypes, const} */
      chrome.commandLinePrivate = {};
    }
    chrome.commandLinePrivate.hasSwitch = (name, callback) => {
      window.setTimeout(() => {
        callback(name in this.flags_);
      }, 0);
    };
  }

  /**
   * Add a switch.
   * @param {string} name of the switch to add.
   */
  addSwitch(name) {
    this.flags_[name] = true;
  }
}

/**
 * Stubs the chrome.storage API.
 */
export class MockChromeStorageAPI {
  constructor() {
    /** @type {Object<?>} */
    this.state = {};

    /** @suppress {const} */
    window.chrome = window.chrome || {};
    /** @suppress {const} */
    window.chrome.runtime = window.chrome.runtime || {};  // For lastError.
    /** @suppress {checkTypes, const} */
    window.chrome.storage = {
      local: {
        get: this.get_.bind(this),
        set: this.set_.bind(this),
      },
      onChanged: {
        addListener: function() {},
      },
    };
  }

  /**
   * @param {Array<string>|string} keys
   * @param {function(Object<?>)} callback
   * @private
   */
  get_(keys, callback) {
    keys = keys instanceof Array ? keys : [keys];
    const result = {};
    keys.forEach((key) => {
      if (key in this.state) {
        result[key] = this.state[key];
      }
    });
    callback(result);
  }

  /**
   * @param {Object<?>} values
   * @param {function()=} opt_callback
   * @private
   */
  set_(values, opt_callback) {
    for (const key in values) {
      this.state[key] = values[key];
    }
    if (opt_callback) {
      opt_callback();
    }
  }
}

/**
 * Mocks out the chrome.fileManagerPrivate.onDirectoryChanged and getSizeStats
 * methods to be useful in unit tests.
 */
export class MockChromeFileManagerPrivateDirectoryChanged {
  constructor() {
    /**
     * Listeners attached to listen for directory changes.
     * @private {!Array<!function(!Event)>}
     * */
    this.listeners_ = [];

    /**
     * Mocked out size stats to return when testing.
     * @private {!Object<string,
     *     (!chrome.fileManagerPrivate.MountPointSizeStats|undefined)>}
     */
    this.sizeStats_ = {};

    /**
     * Mocked out drive quota metadata to return when testing.
     * @private {!chrome.fileManagerPrivate.DriveQuotaMetadata|undefined}
     */
    this.driveQuotaMetadata_ = undefined;

    /** @suppress {const} */
    window.chrome = window.chrome || {};

    /** @suppress {const} */
    window.chrome.fileManagerPrivate = window.chrome.fileManagerPrivate || {};

    /** @suppress {const} */
    window.chrome.fileManagerPrivate.onDirectoryChanged =
        window.chrome.fileManagerPrivate.onDirectoryChanged || {};

    /** @suppress {const} */
    window.chrome.fileManagerPrivate.onDirectoryChanged.addListener =
        this.addListener_.bind(this);

    /** @suppress {const} */
    window.chrome.fileManagerPrivate.onDirectoryChanged.removeListener =
        this.removeListener_.bind(this);

    /** @suppress {const} */
    window.chrome.fileManagerPrivate.getSizeStats =
        this.getSizeStats_.bind(this);

    /** @suppress {const} */
    window.chrome.fileManagerPrivate.getDriveQuotaMetadata =
        this.getDriveQuotaMetadata_.bind(this);

    this.dispatchOnDirectoryChanged =
        this.dispatchOnDirectoryChanged.bind(this);
  }

  /**
   * Store a copy of the listener to emit changes to
   * @param {!function(!Event)} newListener
   * @private
   */
  addListener_(newListener) {
    this.listeners_.push(newListener);
  }

  /**
   *
   * @param {!function(!Event)} listenerToRemove
   * @private
   */
  removeListener_(listenerToRemove) {
    for (let i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i] === listenerToRemove) {
        this.listeners_.splice(i, 1);
        return;
      }
    }
  }

  /**
   * Returns the stubbed out file stats for a directory change.
   * @param {string} volumeId The underlying volumeId requesting size stats for.
   * @param {!function((!chrome.fileManagerPrivate.MountPointSizeStats|undefined))}
   *     callback
   * @private
   */
  getSizeStats_(volumeId, callback) {
    if (!this.sizeStats_[volumeId]) {
      callback(undefined);
      return;
    }

    callback(this.sizeStats_[volumeId]);
  }

  /**
   * Sets the size stats for the volumeId, to return when testing.
   * @param {string} volumeId
   * @param {(!chrome.fileManagerPrivate.MountPointSizeStats|undefined)}
   *     sizeStats
   */
  setVolumeSizeStats(volumeId, sizeStats) {
    this.sizeStats_[volumeId] = sizeStats;
  }

  /**
   * Remove the sizeStats for the volumeId which can emulate getSizeStats
   * returning back undefined.
   * @param {string} volumeId The volumeId to unset.
   */
  unsetVolumeSizeStats(volumeId) {
    delete this.sizeStats_[volumeId];
  }

  /**
   * Returns the stubbed out drive quota metadata for a directory change.
   * @param {Entry} entry
   * @param {!function((!chrome.fileManagerPrivate.DriveQuotaMetadata|undefined))}
   *     callback
   * @private
   */
  getDriveQuotaMetadata_(entry, callback) {
    callback(this.driveQuotaMetadata_);
  }

  /**
   * Sets the drive quota metadata to be returned when testing.
   * @param {(!chrome.fileManagerPrivate.DriveQuotaMetadata|undefined)}
   *     driveQuotaMetadata
   */
  setDriveQuotaMetadata(driveQuotaMetadata) {
    this.driveQuotaMetadata_ = driveQuotaMetadata;
  }

  /**
   * Set the drive quota metadata to undefined to emulate getDriveQuotaMetadata_
   * returning back undefined.
   */
  unsetDriveQuotaMetadata() {
    this.driveQuotaMetadata_ = undefined;
  }

  /**
   * Invoke all the listeners attached to the
   * chrome.fileManagerPrivate.onDirectoryChanged method.
   */
  dispatchOnDirectoryChanged() {
    const event = new Event('fake-event');
    event.entry = 'fake-entry';

    for (const listener of this.listeners_) {
      listener(event);
    }
  }
}

/**
 * Mock for chrome.metricsPrivate.
 *
 * It records the method calls made to chrome.metricsPrivate.
 *
 * Typical usage:
 * const mockMetrics = new MockMetrics();
 *
 * // NOTE: installMockChrome() mocks metricsPrivate by default, which useful
 * when you don't want to check the calls to metrics.
 * installMockChrome({
 *   metricsPrivate: mockMetrics,
 * });
 *
 * // Run the code under test:
 * Then check the calls made to metrics private using either:
 * mockMetrics.apiCalls
 * mockMetrics.metricCalls
 */
export class MockMetrics {
  constructor() {
    /**
     * Maps the API name to every call which is an array of the call arguments.
     * @type {!Object<undefined|!Array<*>>}
     * */
    this.apiCalls = {};

    /**
     * Maps the metric names to every call with its arguments, similar to
     * `apiCalls` but recorded by metric instead of API method.
     * @type {!Object<undefined|!Array<*>>}
     * */
    this.metricCalls = {};

    // The API has this enum which referenced in the code.
    this.MetricTypeType = {
      'HISTOGRAM_LINEAR': 'HISTOGRAM_LINEAR',
    };
  }

  call(apiName, args) {
    console.log(apiName, args);
    this.apiCalls[apiName] = this.apiCalls[apiName] || [];
    this.apiCalls[apiName].push(args);
    if (args.length > 0) {
      let metricName = args[0];
      // Ignore the first position because it's the metric name.
      let metricArgs = args.slice(1);
      // Some APIs uses `metricName` instead of first argument.
      if (metricName.metricName) {
        metricArgs = [metricName, ...metricArgs];
        metricName = metricName.metricName;
      }
      this.metricCalls[metricName] = this.metricCalls[metricName] || [];
      this.metricCalls[metricName].push(metricArgs);
    }
  }

  recordMediumCount(...args) {
    this.call('recordMediumCount', args);
  }
  recordSmallCount(...args) {
    this.call('recordSmallCount', args);
  }
  recordTime(...args) {
    this.call('recordTime', args);
  }
  recordBoolean(...args) {
    this.call('recordBoolean', args);
  }
  recordUserAction(...args) {
    this.call('recordUserAction', args);
  }
  recordValue(...args) {
    this.call('recordValue', args);
  }
  recordInterval(...args) {
    this.call('recordInterval', args);
  }
  recordEnum(...args) {
    this.call('recordEnum', args);
  }
}
