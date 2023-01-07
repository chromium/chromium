// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['storage.js']);

GEN_INCLUDE([
    '../webstore_extension_test_base.js',
    '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
        'callback_helper.js',
    '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
        'mock_storage.js',
]);

/** Test fixture for storage.js. */
HighContrastStorageTest = class extends WebstoreExtensionTest {
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
 * @param {string} key
 * @param {*} expected
 */
function checkStoredValue(key, expected) {
  const storedValue = MockStorage.local_[key];
  assertEquals(typeof (expected), typeof (storedValue));
  assertDeepEquals(expected, storedValue);
}

TEST_F('HighContrastStorageTest', 'DefaultValues', function() {
  assertEquals(true, Storage.enabled);
  assertEquals(SchemeType.INVERTED_COLOR, Storage.baseScheme);
  assertEquals(
      SchemeType.INVERTED_COLOR, Storage.getSiteScheme('default.com'));
});

TEST_F('HighContrastStorageTest', 'SetValues', function() {
  // Enabled
  Storage.enabled = false;
  assertEquals(false, Storage.enabled);
  checkStoredValue(Storage.ENABLED.key, false);

  // Base Scheme
  Storage.baseScheme = SchemeType.YELLOW_ON_BLACK;
  assertEquals(SchemeType.YELLOW_ON_BLACK, Storage.baseScheme);
  checkStoredValue(Storage.SCHEME.key, SchemeType.YELLOW_ON_BLACK);

  // Check that unset sites fall back to the base scheme
  assertEquals(SchemeType.YELLOW_ON_BLACK, Storage.getSiteScheme('unset.com'));

  // Site Scheme
  Storage.setSiteScheme('set.com', SchemeType.GRAYSCALE);
  assertEquals(SchemeType.GRAYSCALE, Storage.getSiteScheme('set.com'));
  checkStoredValue(Storage.SITE_SCHEMES.key, {'set.com': SchemeType.GRAYSCALE});
});

TEST_F('HighContrastStorageTest', 'SetInvalidValues', function() {
  // Enabled
  Storage.enabled = 'x';  // Enabled must be a boolean.
  assertEquals(true, Storage.enabled);
  checkStoredValue(Storage.ENABLED.key, true);

  // Base Scheme
  Storage.baseScheme = 7;  // Scheme must be a SchemeType (number 0-5).
  assertEquals(SchemeType.INVERTED_COLOR, Storage.baseScheme);
  checkStoredValue(Storage.SCHEME.key, SchemeType.INVERTED_COLOR);

  // Site scheme
  Storage.setSiteScheme('invalid.com', 'x');
  assertEquals(SchemeType.INVERTED_COLOR, Storage.getSiteScheme('invalid.com'));
  checkStoredValue(
      Storage.SITE_SCHEMES.key, {'invalid.com': SchemeType.INVERTED_COLOR});
});

TEST_F('HighContrastStorageTest', 'Listeners', function() {
  Storage.enabled = false;
  Storage.ENABLED.listeners.push(this.newCallback(newVal => {
    assertEquals(true, newVal);
    Storage.ENABLED.listeners.pop();
  }));
  Storage.enabled = true;

  Storage.SCHEME.listeners.push(this.newCallback(newVal => {
    assertEquals(SchemeType.INVERTED_GRAYSCALE, newVal);
    Storage.SCHEME.listeners.pop();
  }));
  Storage.baseScheme = SchemeType.INVERTED_GRAYSCALE;

  Storage.SITE_SCHEMES.listeners.push(this.newCallback(newVal => {
    assertEquals('object', typeof (newVal));
    assertEquals(SchemeType.NORMAL, newVal['listener.com']);
    Storage.SITE_SCHEMES.listeners.pop();
  }));
  Storage.setSiteScheme('listener.com', SchemeType.NORMAL);
});

TEST_F('HighContrastStorageTest', 'InitialFetch', function() {
  // Make sure any values from previous tests are cleared.
  MockStorage.local_ = {};

  Storage.enabled = false;
  Storage.setSiteScheme('fetch.com', SchemeType.NORMAL);

  // Simulate re-starting the extension by creating a new instance.
  Storage.instance = new Storage(this.newCallback(() => {
    assertEquals(false, Storage.enabled);
    assertEquals(SchemeType.NORMAL, Storage.getSiteScheme('fetch.com'));

    // Check that unset values are at default.
    assertEquals(SchemeType.INVERTED_COLOR, Storage.baseScheme);
  }));
});

TEST_F('HighContrastStorageTest', 'OnChange', function() {
  Storage.SCHEME.listeners.push(this.newCallback(newVal => {
    assertEquals(SchemeType.INVERTED_GRAYSCALE, newVal);
    Storage.SCHEME.listeners.pop();
  }));

  MockStorage.callOnChangedListeners({
    [Storage.SCHEME.key]: SchemeType.INVERTED_GRAYSCALE });

  // Check that the values were set properly, in addition to the callbacks being
  // called.
  assertEquals(SchemeType.INVERTED_GRAYSCALE, Storage.baseScheme);
});
