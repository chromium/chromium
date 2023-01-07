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
CaretBrowsingStorageTest = class extends WebstoreExtensionTest {
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

TEST_F('CaretBrowsingStorageTest', 'DefaultValues', function() {
  assertEquals(false, Storage.enabled);
  assertEquals(FlourishType.ANIMATE, Storage.onEnable);
  assertEquals(FlourishType.FLASH, Storage.onJump);
});

TEST_F('CaretBrowsingStorageTest', 'SetValues', function() {
  // enabled
  Storage.enabled = true;
  assertEquals(true, Storage.enabled);
  let storedValue = MockStorage.local_[Storage.ENABLED.key];
  assertEquals('boolean', typeof (storedValue));
  assertEquals(true, storedValue);

  // onEnable
  Storage.onEnable = FlourishType.NONE;
  assertEquals(FlourishType.NONE, Storage.onEnable);
  storedValue = MockStorage.local_[Storage.ON_ENABLE.key];
  assertTrue(Object.values(FlourishType).includes(storedValue));
  assertEquals(FlourishType.NONE, storedValue);

  // onJump
  Storage.onJump = FlourishType.ANIMATE;
  assertEquals(FlourishType.ANIMATE, Storage.onJump);
  storedValue = MockStorage.local_[Storage.ON_JUMP.key];
  assertTrue(Object.values(FlourishType).includes(storedValue));
  assertEquals(FlourishType.ANIMATE, storedValue);
});

TEST_F('CaretBrowsingStorageTest', 'SetInvalidValues', function() {
  // enabled
  Storage.enabled = 7;  // enabled must be a boolean
  assertEquals(false, Storage.enabled);
  storedValue = MockStorage.local_[Storage.ENABLED.key];
  assertEquals(false, storedValue);

  // onEnable
  Storage.onEnable = true;  // onEnable must be a FlourishType.
  assertEquals(FlourishType.ANIMATE, Storage.onEnable);
  storedValue = MockStorage.local_[Storage.ON_ENABLE.key];
  assertTrue(Object.values(FlourishType).includes(storedValue));
  assertEquals(FlourishType.ANIMATE, storedValue);

  // onJump
  Storage.onJump = 'x';  // onJump must be a FlourishType.
  assertEquals(FlourishType.FLASH, Storage.onJump);
  storedValue = MockStorage.local_[Storage.ON_JUMP.key];
  assertTrue(Object.values(FlourishType).includes(storedValue));
  assertEquals(FlourishType.FLASH, storedValue);
});

TEST_F('CaretBrowsingStorageTest', 'Listeners', function() {
  Storage.ENABLED.listeners.push(this.newCallback(newVal => {
    assertEquals(true, newVal);
    Storage.ENABLED.listeners.pop();
  }));
  Storage.enabled = true;

  Storage.ON_ENABLE.listeners.push(this.newCallback(newVal => {
    assertEquals(FlourishType.NONE, newVal);
    Storage.ON_ENABLE.listeners.pop();
  }));
  Storage.onEnable = FlourishType.NONE;

  Storage.ON_JUMP.listeners.push(this.newCallback(newVal => {
    assertEquals(FlourishType.ANIMATE, newVal);
    Storage.ON_JUMP.listeners.pop();
  }));
  Storage.onJump = FlourishType.ANIMATE;

});

TEST_F('CaretBrowsingStorageTest', 'InitialFetch', function() {
  // Make sure any values from previous tests are cleared.
  MockStorage.local_ = {};

  Storage.enabled = true;
  Storage.onJump = FlourishType.NONE;

  // Simulate re-starting the extension by creating a new instance.
  Storage.instance = new Storage(this.newCallback(() => {
    assertEquals(FlourishType.NONE, Storage.onJump);
    assertEquals(true, Storage.enabled);

    // Check that unset values are at default.
    assertEquals(FlourishType.ANIMATE, Storage.onEnable);
  }));
});

TEST_F('CaretBrowsingStorageTest', 'OnChange', function() {
  Storage.ON_ENABLE.listeners.push(this.newCallback((newVal) => {
    assertEquals(FlourishType.NONE, newVal);
    Storage.ON_ENABLE.listeners.pop();
  }));


  MockStorage.callOnChangedListeners({
    [Storage.ON_ENABLE.key]: FlourishType.NONE });

  // Check that the value was set properly, in addition to the callbacks being
  // called.
  assertEquals(FlourishType.NONE, Storage.onEnable);
});
