// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Installs a mock object to replace window.chrome in a unit test.
 * @param {!Object} mockChrome
 */
export function installMockChrome(mockChrome) {
  // @ts-ignore: error TS2739: Type '{}' is missing the following properties
  // from type 'typeof chrome': fileManagerPrivate, metricsPrivate, runtime,
  // tabs
  window.chrome = window.chrome || {};
  // @ts-ignore: error TS2339: Property 'metricsPrivate' does not exist on type
  // 'Object'.
  mockChrome.metricsPrivate = mockChrome.metricsPrivate || new MockMetrics();

  const chrome = window.chrome;
  for (const [key, value] of Object.entries(mockChrome)) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type 'typeof chrome'.
    const target = chrome[key] || value;
    Object.assign(target, value);
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type 'typeof chrome'.
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

    // @ts-ignore: error TS2339: Property 'commandLinePrivate' does not exist on
    // type 'typeof chrome'.
    if (!chrome.commandLinePrivate) {
      // @ts-ignore: error TS2339: Property 'commandLinePrivate' does not exist
      // on type 'typeof chrome'.
      chrome.commandLinePrivate = {};
    }
    // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
    // type.
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
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    this.flags_[name] = true;
  }
}

/**
 * Stubs the chrome.storage API.
 */
export class MockChromeStorageAPI {
  constructor() {
    // @ts-ignore: error TS2315: Type 'Object' is not generic.
    /** @type {Object<?>} */
    this.state = {};

    // @ts-ignore: error TS2739: Type '{}' is missing the following properties
    // from type 'typeof chrome': fileManagerPrivate, metricsPrivate, runtime,
    // tabs
    window.chrome = window.chrome || {};
    // @ts-ignore: error TS2739: Type '{}' is missing the following properties
    // from type 'typeof runtime': getURL, getManifest, lastError, id,
    // onMessageExternal
    window.chrome.runtime = window.chrome.runtime || {};  // For lastError.
    // @ts-ignore: error TS2339: Property 'storage' does not exist on type
    // 'typeof chrome'.
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
   * @param {function(Object):void} callback
   * @private
   */
  get_(keys, callback) {
    keys = keys instanceof Array ? keys : [keys];
    const result = {};
    keys.forEach((key) => {
      if (key in this.state) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        result[key] = this.state[key];
      }
    });
    callback(result);
  }

  /**
   * @param {Record<string, any>} values
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
     * @private @type {!Array<!function(!Event):void>}
     * */
    this.listeners_ = [];

    /**
     * Mocked out size stats to return when testing.
     * @private @type {!Object<string,
     *     (!chrome.fileManagerPrivate.MountPointSizeStats|undefined)>}
     */
    this.sizeStats_ = {};

    /**
     * Mocked out drive quota metadata to return when testing.
     * @private @type {!chrome.fileManagerPrivate.DriveQuotaMetadata|undefined}
     */
    this.driveQuotaMetadata_ = undefined;

    // @ts-ignore: error TS2739: Type '{}' is missing the following properties
    // from type 'typeof chrome': fileManagerPrivate, metricsPrivate, runtime,
    // tabs
    window.chrome = window.chrome || {};

    // @ts-ignore: error TS2740: Type '{}' is missing the following properties
    // from type 'typeof fileManagerPrivate': setPreferences,
    // getDriveConnectionState, PreferencesChange, DriveConnectionStateType, and
    // 186 more.
    window.chrome.fileManagerPrivate = window.chrome.fileManagerPrivate || {};

    window.chrome.fileManagerPrivate.onDirectoryChanged =
        window.chrome.fileManagerPrivate.onDirectoryChanged || {};

    window.chrome.fileManagerPrivate.onDirectoryChanged.addListener =
        this.addListener_.bind(this);

    window.chrome.fileManagerPrivate.onDirectoryChanged.removeListener =
        this.removeListener_.bind(this);

    window.chrome.fileManagerPrivate.getSizeStats =
        this.getSizeStats_.bind(this);

    window.chrome.fileManagerPrivate.getDriveQuotaMetadata =
        this.getDriveQuotaMetadata_.bind(this);

    this.dispatchOnDirectoryChanged =
        this.dispatchOnDirectoryChanged.bind(this);
  }

  /**
   * Store a copy of the listener to emit changes to
   * @param {!function(!Event):void} newListener
   * @private
   */
  addListener_(newListener) {
    this.listeners_.push(newListener);
  }

  /**
   *
   * @param {!function(!Event):void} listenerToRemove
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
   * @param {!function((!chrome.fileManagerPrivate.MountPointSizeStats|undefined)):void}
   *     callback
   * @private
   */
  getSizeStats_(volumeId, callback) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    if (!this.sizeStats_[volumeId]) {
      callback(undefined);
      return;
    }

    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    callback(this.sizeStats_[volumeId]);
  }

  /**
   * Sets the size stats for the volumeId, to return when testing.
   * @param {string} volumeId
   * @param {(!chrome.fileManagerPrivate.MountPointSizeStats|undefined)}
   *     sizeStats
   */
  setVolumeSizeStats(volumeId, sizeStats) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    this.sizeStats_[volumeId] = sizeStats;
  }

  /**
   * Remove the sizeStats for the volumeId which can emulate getSizeStats
   * returning back undefined.
   * @param {string} volumeId The volumeId to unset.
   */
  unsetVolumeSizeStats(volumeId) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    delete this.sizeStats_[volumeId];
  }

  /**
   * Returns the stubbed out drive quota metadata for a directory change.
   * @param {Entry} entry
   * @param {!function((!chrome.fileManagerPrivate.DriveQuotaMetadata|undefined)):void}
   *     callback
   * @private
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
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
    // @ts-ignore: error TS2339: Property 'entry' does not exist on type
    // 'Event'.
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
     * @type {!Record<string, undefined|!Array<*>>}
     *
     */
    this.apiCalls = {};

    /**
     * Maps the metric names to every call with its arguments, similar to
     * `apiCalls` but recorded by metric instead of API method.
     * @type {!Record<string, undefined|!Array<*>>}
     *
     */
    this.metricCalls = {};

    // The API has this enum which referenced in the code.
    this.MetricTypeType = {
      'HISTOGRAM_LINEAR': 'HISTOGRAM_LINEAR',
    };
  }

  // @ts-ignore: error TS7006: Parameter 'args' implicitly has an 'any' type.
  call(apiName, args) {
    console.log(apiName, args);
    this.apiCalls[apiName] = this.apiCalls[apiName] || [];
    this.apiCalls[apiName]?.push(args);
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
      this.metricCalls[metricName]?.push(metricArgs);
    }
  }

  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordMediumCount(...args) {
    this.call('recordMediumCount', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordSmallCount(...args) {
    this.call('recordSmallCount', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordTime(...args) {
    this.call('recordTime', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordBoolean(...args) {
    this.call('recordBoolean', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordUserAction(...args) {
    this.call('recordUserAction', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordValue(...args) {
    this.call('recordValue', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordInterval(...args) {
    this.call('recordInterval', args);
  }
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  recordEnum(...args) {
    this.call('recordEnum', args);
  }
}
