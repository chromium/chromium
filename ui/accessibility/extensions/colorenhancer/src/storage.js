// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle both storing values using the chrome.storage API, and
 * fetching/caching values that have been stored that way.
 */
class Storage {
   /** @private */
  constructor() {
    /** @private {number} */
    this.baseDelta_ = Storage.DELTA.defaultValue;

    /** @private {!Object<string, number>} */
    this.siteDeltas_ = Storage.SITE_DELTAS.defaultValue;

    /** @private {number} */
    this.severity_ = Storage.SEVERITY.defaultValue;

    /** @private {!OptionalCvdType} */
    this.type_ = Storage.TYPE.defaultValue;

    /** @private {boolean} */
    this.simulate_ = Storage.SIMULATE.defaultValue;

    /** @private {boolean} */
    this.enable_ = Storage.ENABLE.defaultValue;

    /** @private {!CvdAxis|undefined} */
    this.axis_ = undefined;
  }

  // ======= Public methods =======

  /**
   * @param {function()=} opt_callbackForTesting
   */
  static initialize(opt_callbackForTesting) {
    if (!Storage.instance || opt_callbackForTesting) {
      Storage.instance = new Storage();
      Storage.instance.init_(opt_callbackForTesting);
    }
  }

  /** @return {number} */
  static get baseDelta() { return Storage.instance.baseDelta_; }
  /** @return {number} */
  static get severity() { return Storage.instance.severity_; }
  /** @return {!OptionalCvdType} */
  static get type() { return Storage.instance.type_; }
  /** @return {boolean} */
  static get simulate() { return Storage.instance.simulate_; }
  /** @return {boolean} */
  static get enable() { return Storage.instance.enable_; }
  /** @return {!CvdAxis} */
  static get axis() {
    // on earlier versions axis was not defined and deutan
    // correction used a RED shift. Ensure backwards compatibility
    // with legacy behavior
    if (Storage.instance.axis_ === undefined) {
      if (Storage.instance.type_ === CvdType.DEUTERANOMALY)
        return CvdAxis.RED;
      else
        return CvdAxis.DEFAULT;
    }
    return Storage.instance.axis_;
  }

  /**
   * @param {string} site
   * @return {number}
   */
  static getSiteDelta(site) {
    const delta = Storage.instance.siteDeltas_[site];
    if (Storage.DELTA.validate(delta)) {
      return delta;
    }
    Storage.setSiteDelta(site, Storage.baseDelta);
    return Storage.baseDelta;
  }

  /** @param {number} newDelta */
  static set baseDelta(newDelta) {
    Storage.instance.setOrResetValue_(Storage.DELTA, newDelta);
    Storage.instance.store_(Storage.DELTA);
  }

  /** @param {number} newSeverity */
  static set severity(newSeverity) {
    Storage.instance.setOrResetValue_(Storage.SEVERITY, newSeverity);
    Storage.instance.store_(Storage.SEVERITY);
  }

  /** @param {!CvdType} newCvdType */
  static set type(newCvdType) {
    Storage.instance.setOrResetValue_(Storage.TYPE, newCvdType);
    Storage.instance.store_(Storage.TYPE);
  }

  /** @param {boolean} newValue */
  static set simulate(newValue) {
    Storage.instance.setOrResetValue_(Storage.SIMULATE, newValue);
    Storage.instance.store_(Storage.SIMULATE);
  }

  /** @param {boolean} newValue */
  static set enable(newValue) {
    Storage.instance.setOrResetValue_(Storage.ENABLE, newValue);
    Storage.instance.store_(Storage.ENABLE);
  }

  /**
   * @param {string} site
   * @param {number} delta
   */
  static setSiteDelta(site, delta) {
    if (Storage.DELTA.validate(delta)) {
      Storage.instance.siteDeltas_[site] = delta;
    } else {
      Storage.instance.siteDeltas_[site] = Storage.baseDelta;
    }
    Storage.instance.store_(Storage.SITE_DELTAS);
  }

  /** @param {!CvdAxis} newCvdAxis */
  static set axis(newCvdAxis) {
    Storage.instance.setOrResetValue_(Storage.AXIS, newCvdAxis);
    Storage.instance.store_(Storage.AXIS);
  }

  // ======== Private Methods ========

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
   * @param {!Storage.Value} value
   * @private
   */
  store_(value) {
    chrome.storage.local.set({ [value.key]: value.get() });
  }

  /**
   * @param {function()} opt_callback
   * @private
   */
  init_(opt_callback) {
    chrome.storage.onChanged.addListener(this.onChange_.bind(this));

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
      this.setOrResetValue_(value, changes[value.key].newValue);
    }
  }

  // ======== Stored Values ========

  /** @const {string} */
  static INVALID_TYPE_PLACEHOLDER = '';

  /**
   * @typedef {{
   *     key: string,
   *     defaultValue: *,
   *     validate: function(*): boolean,
   *     get: function(): *,
   *     set: function(*),
   *     reset: function(),
   *     listeners: !Array<function(*)>
   * }}
   */
  static Value;

  /** @const {!Storage.Value} */
  static DELTA = {
    key: 'cvd_delta',
    defaultValue: 0.5,
    validate: (delta) => delta >= 0 && delta <= 1,
    get: () => Storage.instance.baseDelta_,
    set: (delta) => Storage.instance.baseDelta_ = delta,
    reset: () => Storage.instance.baseDelta_ = Storage.DELTA.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static SITE_DELTAS = {
    key: 'cvd_site_delta',
    defaultValue: {},
    validate: (siteDeltas) => typeof (siteDeltas) === 'object',
    get: () => Storage.instance.siteDeltas_,
    set: (siteDeltas) => {
      for (const site of Object.keys(siteDeltas)) {
        if (Storage.DELTA.validate(siteDeltas[site])) {
          Storage.instance.siteDeltas_[site] = siteDeltas[site];
        }
      }
    },
    reset: () => {} /* Do nothing */,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static SEVERITY = {
    key: 'cvd_severity',
    defaultValue: 1.0,
    validate: (severity) => severity >= 0 && severity <= 1,
    get: () => Storage.instance.severity_,
    set: (severity) => Storage.instance.severity_ = severity,
    reset: () => Storage.instance.severity_ = Storage.SEVERITY.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static TYPE = {
    key: 'cvd_type',
    defaultValue: Storage.INVALID_TYPE_PLACEHOLDER,
    validate: (type) => Object.values(CvdType).includes(type),
    get: () => Storage.instance.type_,
    set: (type) => Storage.instance.type_ = type,
    reset: () => Storage.instance.type_ = Storage.TYPE.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static SIMULATE = {
    key: 'cvd_simulate',
    defaultValue: false,
    validate: (simulate) => typeof (simulate) === 'boolean',
    get: () => Storage.instance.simulate_,
    set: (simulate) => Storage.instance.simulate_ = simulate,
    reset: () => Storage.instance.simulate_ = Storage.SIMULATE.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static ENABLE = {
    key: 'cvd_enable',
    defaultValue: false,
    validate: (enable) => typeof (enable) === 'boolean',
    get: () => Storage.instance.enable_,
    set: (enable) => Storage.instance.enable_ = enable,
    reset: () => Storage.instance.enable_ = Storage.ENABLE.defaultValue,
    listeners: [],
  };

  /** @const {!Storage.Value} */
  static AXIS = {
    key: 'cvd_axis',
    defaultValue: 'DEFAULT',
    validate: (axis) => Object.values(CvdAxis).includes(axis),
    get: () => Storage.instance.axis_,
    set: (axis) => Storage.instance.axis_ = axis,
    reset: () => Storage.instance.axis_ = Storage.AXIS.defaultValue,
    listeners: [],
  };

  /** @const {!Array<!Storage.Value>} */
  static ALL_VALUES = [
      Storage.DELTA, Storage.SITE_DELTAS, Storage.SEVERITY, Storage.TYPE,
      Storage.SIMULATE, Storage.ENABLE, Storage.AXIS,
  ];
}
