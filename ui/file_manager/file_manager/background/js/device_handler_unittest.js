// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/** @type {!MockVolumeManager} */
let volumeManager;

/** @type {!DeviceHandler} */
let deviceHandler;

/** Mock chrome APIs. */
let mockChrome;

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordEnum: function() {},
};

// Set up the test components.
function setUp() {
  // Set up string assets.
  window.loadTimeData.data = {
    DEVICE_UNSUPPORTED_MESSAGE: 'DEVICE_UNSUPPORTED: $1',
    DEVICE_UNKNOWN_MESSAGE: 'DEVICE_UNKNOWN: $1',
    MULTIPART_DEVICE_UNSUPPORTED_MESSAGE: 'MULTIPART_DEVICE_UNSUPPORTED: $1',
  };
  window.loadTimeData.getString = id => {
    return window.loadTimeData.data_[id] || id;
  };
  window.loadTimeData.getBoolean = id => {
    return id === 'ARC_USB_STORAGE_UI_ENABLED' ? true : false;
  };
  window.loadTimeData.valueExists = id => {
    return id === 'ARC_USB_STORAGE_UI_ENABLED';
  };

  setupChromeApis();
  volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  deviceHandler = new DeviceHandler();
}

function setUpInIncognitoContext() {
  mockChrome.extension.inIncognitoContext = true;
}

function testGoodDevice(callback) {
  // Turn off ARC so that the notification won't show the "OPEN SETTINGS"
  // button.
  mockChrome.fileManagerPrivate.arcEnabledPref = false;

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  // Since arcEnabled is false here, the notification doesn't mention ARC.
  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        const options = notifications['deviceNavigation:/device/path'];
        assertEquals('REMOVABLE_DEVICE_NAVIGATION_MESSAGE', options.message);
        assertTrue(options.isClickable);
      }),
      callback);
}

function testGoodDeviceWithAllowPlayStoreMessage(callback) {
  // Turn on ARC so that the notification shows the "OPEN SETTINGS" button.
  mockChrome.fileManagerPrivate.arcEnabledPref = true;
  // Turn off the ARC pref so that the notification shows the "Allow Play Store
  // applications ..." label.
  mockChrome.fileManagerPrivate.arcRemovableMediaAccessEnabledPref = false;

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        const options = notifications['deviceNavigationAppAccess:/device/path'];
        assertEquals(
            'REMOVABLE_DEVICE_NAVIGATION_MESSAGE ' +
                'REMOVABLE_DEVICE_ALLOW_PLAY_STORE_ACCESS_MESSAGE',
            options.message);
        assertTrue(options.isClickable);
      }),
      callback);
}

function testGoodDeviceWithPlayStoreAppsHaveAccessMessage(callback) {
  // Turn on ARC so that the notification shows the "OPEN SETTINGS" button.
  mockChrome.fileManagerPrivate.arcEnabledPref = true;
  // Turn on the ARC pref so that the notification shows the "Play Store apps
  // have ..." label.
  mockChrome.fileManagerPrivate.arcRemovableMediaAccessEnabledPref = true;

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  // Since arcRemovableMediaAccessEnabled is true here, "Play Store apps have
  // access to ..." message is shown.
  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        const options = notifications['deviceNavigationAppAccess:/device/path'];
        assertEquals(
            'REMOVABLE_DEVICE_NAVIGATION_MESSAGE ' +
                'REMOVABLE_DEVICE_PLAY_STORE_APPS_HAVE_ACCESS_MESSAGE',
            options.message);
        console.log(options.message);
        assertTrue(options.isClickable);
      }),
      callback);
}

function testRemovableMediaDeviceWithImportEnabled(callback) {
  const storage = new MockChromeStorageAPI();

  setupFileSystem(VolumeManagerCommon.VolumeType.REMOVABLE, 'blabbity', [
    '/DCIM/',
    '/DCIM/grandma.jpg',
  ]);

  const resolver = new importer.Resolver();

  // Handle media device navigation requests.
  deviceHandler.addEventListener(
      DeviceHandler.VOLUME_NAVIGATION_REQUESTED, event => {
        resolver.resolve(event);
      });

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {volumeId: 'blabbity', deviceType: 'usb'},
    shouldNotify: true
  });

  reportPromise(
      resolver.promise.then(event => {
        assertEquals('blabbity', event.volumeId);
      }),
      callback);
}

function testMtpMediaDeviceWithImportEnabled(callback) {
  const storage = new MockChromeStorageAPI();

  setupFileSystem(VolumeManagerCommon.VolumeType.MTP, 'blabbity', [
    '/dcim/',
    '/dcim/grandpa.jpg',
  ]);

  const resolver = new importer.Resolver();

  // Handle media device navigation requests.
  deviceHandler.addEventListener(
      DeviceHandler.VOLUME_NAVIGATION_REQUESTED, event => {
        resolver.resolve(event);
      });

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {volumeId: 'blabbity', deviceType: 'mtp'},
    shouldNotify: true
  });

  reportPromise(
      resolver.promise.then(event => {
        assertEquals('blabbity', event.volumeId);
      }),
      callback);
}

function testGoodDeviceNotNavigated() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: false
  });

  assertEquals(0, Object.keys(mockChrome.notifications.items).length);
  assertFalse(mockChrome.notifications.resolver.settled);
}

function testGoodDeviceWithBadParent(callback) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertFalse(!!notifications['device:/device/path']);
        assertEquals(
            'DEVICE_UNKNOWN: label',
            notifications['deviceFail:/device/path'].message);
      }),
      callback);
}

function testGoodDeviceWithBadParent_DuplicateMount(callback) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  // Mounting the same device repeatedly should produce only
  // a single notification.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        assertEquals(
            'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
            notifications['deviceNavigation:/device/path'].message);
      }),
      callback);
}

function testUnsupportedDevice(callback) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertFalse(!!mockChrome.notifications.items['device:/device/path']);
        assertEquals(
            'DEVICE_UNSUPPORTED: label',
            mockChrome.notifications.items['deviceFail:/device/path'].message);
      }),
      callback);
}

function testUnknownDevice(callback) {
  // Emulate adding a device which has unknown filesystem.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unknown_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      isReadOnly: false,
      deviceType: 'usb',
      devicePath: '/device/path',
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertFalse(!!mockChrome.notifications.items['device:/device/path']);
        const item = mockChrome.notifications.items['deviceFail:/device/path'];
        assertEquals('DEVICE_UNKNOWN_DEFAULT_MESSAGE', item.message);
        // "Format device" button should appear.
        assertEquals('DEVICE_UNKNOWN_BUTTON_LABEL', item.buttons[0].title);
      }),
      callback);
}

function testUnknownReadonlyDevice(callback) {
  // Emulate adding a device which has unknown filesystem but is read-only.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unknown_filesystem',
    volumeMetadata: {
      isParentDevice: true,
      isReadOnly: true,
      deviceType: 'sd',
      devicePath: '/device/path',
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertFalse(!!mockChrome.notifications.items['device:/device/path']);
        const item = mockChrome.notifications.items['deviceFail:/device/path'];
        assertEquals('DEVICE_UNKNOWN_DEFAULT_MESSAGE', item.message);
        // "Format device" button should not appear.
        assertFalse(!!item.buttons);
      }),
      callback);
}

function testUnsupportedWithUnknownParentReplacesNotification() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  assertEquals(
      'DEVICE_UNKNOWN: label',
      mockChrome.notifications.items['deviceFail:/device/path'].message);

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNSUPPORTED: label',
      mockChrome.notifications.items['deviceFail:/device/path'].message);
}

function testMountPartialSuccess(callback) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise
          .then(notifications => {
            assertEquals(1, Object.keys(notifications).length);
            assertEquals(
                'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
                notifications['deviceNavigation:/device/path'].message);
          })
          .then(() => {
            mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
              eventType: 'mount',
              status: 'error_unsupported_filesystem',
              volumeMetadata: {
                isParentDevice: false,
                deviceType: 'usb',
                devicePath: '/device/path',
                deviceLabel: 'label'
              },
              shouldNotify: true
            });
          })
          .then(() => {
            const notifications = mockChrome.notifications.items;
            assertEquals(2, Object.keys(notifications).length);
            assertEquals(
                'MULTIPART_DEVICE_UNSUPPORTED: label',
                notifications['deviceFail:/device/path'].message);
          }),
      callback);
}

function testUnknown(callback) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unknown',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        assertEquals(
            'DEVICE_UNKNOWN: label',
            notifications['deviceFail:/device/path'].message);
      }),
      callback);
}

function testNonASCIILabel(callback) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      // "RA (U+30E9) BE (U+30D9) RU (U+30EB)" in Katakana letters.
      deviceLabel: '\u30E9\u30D9\u30EB'
    },
    shouldNotify: true
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        assertEquals(
            'DEVICE_UNKNOWN: \u30E9\u30D9\u30EB',
            notifications['deviceFail:/device/path'].message);
      }),
      callback);
}

function testMulitpleFail() {
  // The first parent error.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNKNOWN: label',
      mockChrome.notifications.items['deviceFail:/device/path'].message);

  // The first child error that replaces the parent error.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNKNOWN: label',
      mockChrome.notifications.items['deviceFail:/device/path'].message);

  // The second child error that turns to a multi-partition error.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'MULTIPART_DEVICE_UNSUPPORTED: label',
      mockChrome.notifications.items['deviceFail:/device/path'].message);

  // The third child error that should be ignored because the error message does
  // not changed.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'MULTIPART_DEVICE_UNSUPPORTED: label',
      mockChrome.notifications.items['deviceFail:/device/path'].message);
}

function testDisabledDevice() {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'disabled', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'EXTERNAL_STORAGE_DISABLED_MESSAGE',
      mockChrome.notifications.items['deviceFail:/device/path'].message);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'removed', devicePath: '/device/path'});
  assertEquals(0, Object.keys(mockChrome.notifications.items).length);
}

function testFormatSucceeded() {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'FORMATTING_OF_DEVICE_PENDING_MESSAGE',
      mockChrome.notifications.items['formatStart:/device/path'].message);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_success', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'FORMATTING_FINISHED_SUCCESS_MESSAGE',
      mockChrome.notifications.items['formatSuccess:/device/path'].message);
}

function testFormatFailed() {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'FORMATTING_OF_DEVICE_PENDING_MESSAGE',
      mockChrome.notifications.items['formatStart:/device/path'].message);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_fail', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'FORMATTING_FINISHED_FAILURE_MESSAGE',
      mockChrome.notifications.items['formatFail:/device/path'].message);
}

function testRenameSucceeded() {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_start', devicePath: '/device/path'});
  assertEquals(0, Object.keys(mockChrome.notifications.items).length);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_success', devicePath: '/device/path'});
  assertEquals(0, Object.keys(mockChrome.notifications.items).length);
}

function testRenameFailed() {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_start', devicePath: '/device/path'});
  assertEquals(0, Object.keys(mockChrome.notifications.items).length);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_fail', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'RENAMING_OF_DEVICE_FINISHED_FAILURE_MESSAGE',
      mockChrome.notifications.items['renameFail:/device/path'].message);
}

function testDeviceHardUnplugged() {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'hard_unplugged', devicePath: '/device/path'});
  assertEquals(1, Object.keys(mockChrome.notifications.items).length);
  assertEquals(
      'DEVICE_HARD_UNPLUGGED_MESSAGE',
      mockChrome.notifications.items['hardUnplugged:/device/path'].message);
}

function testNotificationClicked(callback) {
  const devicePath = '/device/path';
  const notificationId = 'deviceNavigation:' + devicePath;

  // Add a listener for navigation-requested events.
  const resolver = new importer.Resolver();
  deviceHandler.addEventListener(
      DeviceHandler.VOLUME_NAVIGATION_REQUESTED, event => {
        resolver.resolve(event);
      });

  // Call the notification-body-clicked handler and check that the
  // navigation-requested event is dispatched.
  mockChrome.notifications.onClicked.dispatch(notificationId);
  reportPromise(
      resolver.promise.then(event => {
        assertEquals(null, event.volumeId);
        assertEquals(devicePath, event.devicePath);
        assertEquals(null, event.filePath);
      }),
      callback);
}

function testMiscMessagesInIncognito() {
  setUpInIncognitoContext();
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path'});
  // No notification sent by this instance in incognito context.
  assertEquals(0, Object.keys(mockChrome.notifications.items).length);
  assertFalse(mockChrome.notifications.resolver.settled);
}

function testMountCompleteInIncognito() {
  setUpInIncognitoContext();
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });

  assertEquals(0, Object.keys(mockChrome.notifications.items).length);
  // TODO(yamaguchi): I think this test is incomplete.
  // This looks as if notification is not generated yet because the promise
  // is not settled yet. Same for testGoodDeviceNotNavigated.
  assertFalse(mockChrome.notifications.resolver.settled);
}

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string} volumeId
 * @param {!Array<string>} fileNames
 * @return {!VolumeInfo}
 */
function setupFileSystem(volumeType, volumeId, fileNames) {
  /** @type {!MockVolumeManager}*/
  const mockVolumeManager = volumeManager;
  const volumeInfo = mockVolumeManager.createVolumeInfo(
      volumeType, volumeId, 'A volume known as ' + volumeId);
  assertTrue(!!volumeInfo);
  const mockFileSystem = /** @type {!MockFileSystem} */
      (volumeInfo.fileSystem);
  mockFileSystem.populate(fileNames);
  return volumeInfo;
}

function setupChromeApis() {
  // Mock chrome APIs.
  mockChrome = {
    extension: {
      inIncognitoContext: false,
    },
    fileManagerPrivate: {
      onDeviceChanged: {
        dispatch: null,
        addListener: function(listener) {
          mockChrome.fileManagerPrivate.onDeviceChanged.dispatch = listener;
        }
      },
      onMountCompleted: {
        dispatch: null,
        addListener: function(listener) {
          mockChrome.fileManagerPrivate.onMountCompleted.dispatch = listener;
        }
      },
      getProfiles: function(callback) {
        callback([{profileId: 'userid@xyz.domain.org'}]);
      },
      getPreferences: function(callback) {
        callback({
          arcEnabled: mockChrome.fileManagerPrivate.arcEnabledPref,
          arcRemovableMediaAccessEnabled:
              mockChrome.fileManagerPrivate.arcRemovableMediaAccessEnabledPref
        });
      },
      arcEnabledPref: false,
      arcRemovableMediaAccessEnabledPref: true,
    },
    i18n: {
      getUILanguage: function() {
        return 'en-US';
      }
    },
    notifications: {
      resolver: new importer.Resolver(),
      promise: null,
      create: function(id, params, callback) {
        mockChrome.notifications.promise =
            mockChrome.notifications.resolver.promise;
        mockChrome.notifications.items[id] = params;
        if (!mockChrome.notifications.resolver.settled) {
          mockChrome.notifications.resolver.resolve(
              mockChrome.notifications.items);
        }
        callback();
      },
      clear: function(id, callback) {
        delete mockChrome.notifications.items[id];
        callback();
      },
      items: {},
      onButtonClicked: {
        dispatch: null,
        addListener: function(listener) {
          mockChrome.notifications.onButtonClicked.dispatch = listener;
        }
      },
      onClicked: {
        dispatch: null,
        addListener: function(listener) {
          mockChrome.notifications.onClicked.dispatch = listener;
        }
      },
      getAll: function(callback) {
        callback([]);
      }
    },
    runtime: {
      getURL: function(path) {
        return path;
      },
      onStartup: {addListener: function() {}}
    }
  };

  installMockChrome(mockChrome);
}
