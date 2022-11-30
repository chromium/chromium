// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['cvd_type.js', 'storage.js']);

GEN_INCLUDE([
    '../../webstore_extension_test_base.js',
    '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
    'callback_helper.js',
    '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
    'mock_storage.js',
]);

/** Test fixture for storage.js. */
ColorEnhancerStorageTest = class extends WebstoreExtensionTest {
  /** @override */
  setUp() {
    this.callbackHelper_ = new CallbackHelper(this);
    chrome.storage = MockStorage;
    Storage.initialize();
  }

  /**
   * Increments a counter, to wait for all callbacks to be completed before
   * finishing the test.
   * @param {Function=} opt_callback
   * @return {Function}
   */
  newCallback(opt_callback) {
    return this.callbackHelper_.wrap(opt_callback);
  }
};

/**
 * @param {number} expected
 * @param {number} actual
 */
function checkFloatValue(expected, actual) {
  const difference = Math.abs(expected - actual);
  assertLE(difference, 0.01, 'Floating point number is not the expected value');
}

TEST_F('ColorEnhancerStorageTest', 'DefaultValues', function() {
  checkFloatValue(0.5, Storage.baseDelta);
  checkFloatValue(0.5, Storage.getSiteDelta('default.com'));
  checkFloatValue(1.0, Storage.severity);
  assertEquals(Storage.INVALID_TYPE_PLACEHOLDER, Storage.type);
  assertEquals(CvdAxis.DEFAULT, Storage.axis);
  assertEquals(false, Storage.simulate);
  assertEquals(false, Storage.enable);
});

TEST_F('ColorEnhancerStorageTest', 'SetValues', function() {
  // Default delta
  Storage.baseDelta = 0.8;
  checkFloatValue(0.8, Storage.baseDelta);
  let storedValue = MockStorage.local_[Storage.DELTA.key];
  assertEquals('number', typeof (storedValue));
  checkFloatValue(0.8, storedValue);

  // Check that unset sites fall back to the default delta.
  checkFloatValue(0.8, Storage.getSiteDelta('unset.com'));

  // Severity
  Storage.severity = 0.2;
  checkFloatValue(0.2, Storage.severity);
  storedValue = MockStorage.local_[Storage.SEVERITY.key];
  assertEquals('number', typeof (storedValue));
  checkFloatValue(0.2, storedValue);

  // Type
  Storage.type = CvdType.PROTANOMALY;
  assertEquals(CvdType.PROTANOMALY, Storage.type);
  storedValue = MockStorage.local_[Storage.TYPE.key];
  assertEquals('string', typeof (storedValue));
  assertEquals(CvdType.PROTANOMALY, storedValue);

  // Axis
  Storage.axis = CvdAxis.RED;
  assertEquals(CvdAxis.RED, Storage.axis);
  storedValue = MockStorage.local_[Storage.AXIS.key];
  assertEquals('string', typeof (storedValue));
  assertEquals(CvdAxis.RED, storedValue);

  // Simulate
  Storage.simulate = true;
  assertEquals(true, Storage.simulate);
  storedValue = MockStorage.local_[Storage.SIMULATE.key];
  assertEquals('boolean', typeof (storedValue));
  assertEquals(true, storedValue);

  // Enable
  Storage.enable = true;
  assertEquals(true, Storage.enable);
  storedValue = MockStorage.local_[Storage.ENABLE.key];
  assertEquals('boolean', typeof (storedValue));
  assertEquals(true, storedValue);

  // Site delta
  Storage.setSiteDelta('set.com', 0.9);
  checkFloatValue(0.9, Storage.getSiteDelta('set.com'));
  storedValue = MockStorage.local_[Storage.SITE_DELTAS.key];
  assertEquals('object', typeof (storedValue));
  assertEquals('number', typeof (storedValue['set.com']));
  checkFloatValue(0.9, storedValue['set.com']);
});

TEST_F('ColorEnhancerStorageTest', 'SetInvalidValues', function() {
  // Default delta
  Storage.baseDelta = 1.8;  // Delta must be between 0 and 1.
  checkFloatValue(0.5, Storage.baseDelta);
  let storedValue = MockStorage.local_[Storage.DELTA.key];
  assertEquals('number', typeof (storedValue));
  checkFloatValue(0.5, storedValue);

  // Severity
  Storage.severity = 2;  // Severity must be between 0 and 1.
  checkFloatValue(1.0, Storage.severity);
  storedValue = MockStorage.local_[Storage.SEVERITY.key];
  assertEquals('number', typeof (storedValue));
  checkFloatValue(1.0, storedValue);

  // Type
  Storage.type = 'something else';  // Type must be a CvdType.
  assertEquals(Storage.INVALID_TYPE_PLACEHOLDER, Storage.type);
  storedValue = MockStorage.local_[Storage.TYPE.key];
  assertEquals(Storage.INVALID_TYPE_PLACEHOLDER, storedValue);

  // Axis
  Storage.axis = 'PURPLE';  // Axis must be a CvdAxis.
  assertEquals(CvdAxis.DEFAULT, Storage.axis);
  storedValue = MockStorage.local_[Storage.AXIS.key];
  assertEquals(CvdAxis.DEFAULT, storedValue);

  // Simulate
  Storage.simulate = 7;  // Simulate must be a boolean.
  assertEquals(false, Storage.simulate);
  storedValue = MockStorage.local_[Storage.SIMULATE.key];
  assertEquals('boolean', typeof (storedValue));
  assertEquals(false, storedValue);

  // Enable
  Storage.enable = 'x';  // Enable must be a boolean
  assertEquals(false, Storage.enable);
  storedValue = MockStorage.local_[Storage.ENABLE.key];
  assertEquals('boolean', typeof (storedValue));
  assertEquals(false, storedValue);

  // Site delta
  Storage.setSiteDelta('invalid.com', 9.9);
  checkFloatValue(0.5, Storage.getSiteDelta('invalid.com'));
  storedValue = MockStorage.local_[Storage.SITE_DELTAS.key];
  assertEquals('object', typeof (storedValue));
  assertEquals('number', typeof (storedValue['invalid.com']));
  checkFloatValue(0.5, storedValue['invalid.com']);
});

TEST_F('ColorEnhancerStorageTest', 'Listeners', function() {
  Storage.DELTA.listeners.push(this.newCallback(newVal => {
    checkFloatValue(0.8, newVal);
    Storage.DELTA.listeners.pop();
  }));
  Storage.baseDelta = 0.8;

  // When an invalid value is provided, Storage resets to the default, and
  // listeners are called.
  Storage.SEVERITY.listeners.push(this.newCallback(newVal => {
    checkFloatValue(0.5, newVal);
    Storage.SEVERITY.listeners.pop();
  }));
  Storage.severity = 7.2;

  Storage.TYPE.listeners.push(this.newCallback(newVal => {
    assertEquals(CvdType.PROTANOMLAY, newVal);
    Storage.TYPE.listeners.pop();
  }));
  Storage.type = CvdType.PROTANOMALY;

  Storage.AXIS.listeners.push(this.newCallback(newVal => {
    assertEquals(CvdAxis.RED, newVal);
    Storage.AXIS.listeners.pop();
  }));
  Storage.type = CvdAxis.RED;

  Storage.SIMULATE.listeners.push(
      this.newCallback(newVal => assertEquals(true, newVal)));
  Storage.simulate = true;
  // Setting to the same value already set should not call the listener.
  Storage.simulate = true;
  // Clean up the listener. Note: if callback is async in testing, you'll need
  // to find another place for this.
  Storage.SIMULATE.listeners.pop();

  Storage.ENABLE.listeners.push(this.newCallback(newVal => {
    assertEquals(true, newVal);
    Storage.ENABLE.listeners.pop();
  }));
  Storage.enable = true;

  Storage.SITE_DELTAS.listeners.push(this.newCallback(newVal => {
    assertEquals('object', typeof (newVal));
    assertEquals('number', typeof (newVal['listener.com']));
    checkFloatValue(0.1, newVal['listener.com']);
    Storage.SITE_DELTAS.listeners.pop();
  }));
  Storage.setSiteDelta('listener.com', 0.1);
});

TEST_F('ColorEnhancerStorageTest', 'InitialFetch', function() {
  // Make sure any values from previous tests are cleared.
  MockStorage.local_ = {};

  Storage.baseDelta = 0.7;
  Storage.type = CvdType.PROTANOMALY;
  Storage.enable = true;
  Storage.setSiteDelta('fetch.com', 0.2);

  // Simulate re-starting the extension by creating a new instance.
  Storage.initialize(this.newCallback(() => {
    checkFloatValue(0.7, Storage.baseDelta);
    assertEquals(CvdType.PROTANOMALY, Storage.type);
    assertEquals(true, Storage.enable);
    checkFloatValue(0.2, Storage.getSiteDelta('fetch.com'));

    // Check that unset values are at default.
    checkFloatValue(1.0, Storage.severity);
    assertEquals(false, Storage.simulate);
    assertEquals(CvdAxis.DEFAULT, Storage.axis);
  }));
});

TEST_F('ColorEnhancerStorageTest', 'LegacyFetch', function() {
  // Make sure any values from previous tests are cleared.
  MockStorage.local_ = {};

  Storage.baseDelta = 0.7;
  Storage.type = CvdType.DEUTERANOMALY;
  Storage.enable = true;
  Storage.setSiteDelta('fetch.com', 0.2);

  delete MockStorage.local_[Storage.AXIS.key]

  // Simulate re-starting the extension by creating a new instance.
  Storage.initialize(this.newCallback(() => {
    checkFloatValue(0.7, Storage.baseDelta);
    assertEquals(CvdType.DEUTERANOMALY, Storage.type);
    assertEquals(true, Storage.enable);
    checkFloatValue(0.2, Storage.getSiteDelta('fetch.com'));

    // Check that Axis uses legacy value
    assertEquals(CvdAxis.RED, Storage.axis);
  }));
});

TEST_F('ColorEnhancerStorageTest', 'OnChange', function() {
  Storage.SEVERITY.listeners.push(this.newCallback((newVal) => {
    assertEquals(0.35, newVal);
    Storage.SEVERITY.listeners.pop();
  }));

  // Because the listener is only called if the value changes, make sure the
  // value is false beforehand.
  Storage.simulate = false;
  Storage.SIMULATE.listeners.push(this.newCallback((newVal) => {
    assertEquals(true, newVal);
    Storage.SIMULATE.listeners.pop();
  }));

  Storage.AXIS.listeners.push(this.newCallback((newVal) => {
    assertEquals(CvdAxis.RED, newVal);
    Storage.AXIS.listeners.pop();
  }));


  MockStorage.callOnChangedListeners({
    [Storage.SEVERITY.key]: 0.35,
    [Storage.SIMULATE.key]: true,
    [Storage.AXIS.key]: CvdAxis.RED,
  });

  // Check that the values were set properly, in addition to the callbacks being
  // called.
  checkFloatValue(0.35, Storage.severity);
  assertEquals(true, Storage.simulate);
  assertEquals(CvdAxis.RED, Storage.axis);
});
