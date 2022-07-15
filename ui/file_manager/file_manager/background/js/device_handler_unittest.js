// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {importer} from '../../common/js/importer_common.js';
import {metrics} from '../../common/js/metrics.js';
import {installMockChrome, MockChromeStorageAPI} from '../../common/js/mock_chrome.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise, waitUntil} from '../../common/js/test_error_reporting.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {DeviceHandler} from './device_handler.js';
import {MockProgressCenter} from './mock_progress_center.js';
import {MockVolumeManager} from './mock_volume_manager.js';

/** @type {!MockVolumeManager} */
let volumeManager;

/** @type {!MockProgressCenter} */
let progressCenter;

/** @type {!DeviceHandler} */
let deviceHandler;

/** Mock chrome APIs. */
let mockChrome;

/**
 * @type {boolean}
 */
let swaEnabledState = false;

/**
 * @type {function(): boolean}
 */
let restoreIsSwaEnabled;

/**
 * Mock metrics.
 * @param {string} name
 * @param {*} value
 * @param {Array<*>|number=} opt_validValues
 */
metrics.recordEnum = function(name, value, opt_validValues) {};

/**
 * @type {function(function():void): !Promise<boolean>}
 */
let originalDoIfPrimaryContext;

/**
 * Helper function for reporting errors from a promise handling code.
 * @param {string} message
 * @param {function(boolean):void} done
 * @return {!Promise<boolean>} Always Promise.resolve(false);
 */
function reportError(message, done) {
  console.error(message);
  done(/*error=*/ true);
  return Promise.resolve(false);
}


// Set up the test components.
export function setUp() {
  // store the doIfPrimary original function, as we frequently modify it.
  originalDoIfPrimaryContext = util.doIfPrimaryContext;
  // Set up string assets.
  loadTimeData.resetForTesting({
    DEVICE_UNSUPPORTED_MESSAGE: 'DEVICE_UNSUPPORTED: $1',
    DEVICE_UNKNOWN_MESSAGE: 'DEVICE_UNKNOWN: $1',
    MULTIPART_DEVICE_UNSUPPORTED_MESSAGE: 'MULTIPART_DEVICE_UNSUPPORTED: $1',
    FORMAT_PROGRESS_MESSAGE: 'FORMAT_PROGRESS_MESSAGE: $1',
    FORMAT_SUCCESS_MESSAGE: 'FORMAT_SUCCESS_MESSAGE: $1',
    FORMAT_FAILURE_MESSAGE: 'FORMAT_FAILURE_MESSAGE: $1',
  });
  loadTimeData.getString = id => {
    return loadTimeData.data_[id] || id;
  };
  loadTimeData.getBoolean = id => {
    return id === 'ARC_USB_STORAGE_UI_ENABLED' ? true : false;
  };
  loadTimeData.valueExists = id => {
    return id === 'ARC_USB_STORAGE_UI_ENABLED';
  };

  setupChromeApis();
  volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  progressCenter = new MockProgressCenter();

  restoreIsSwaEnabled = util.isSwaEnabled;
  util.isSwaEnabled = () => swaEnabledState;
  window.isSWA = false;
  deviceHandler = new DeviceHandler(progressCenter);
}

export function tearDown() {
  util.isSwaEnabled = restoreIsSwaEnabled;
  swaEnabledState = false;
  util.doIfPrimaryContext = originalDoIfPrimaryContext;
}

function setUpInIncognitoContext() {
  mockChrome.extension.inIncognitoContext = true;
}

export async function testGoodDevice(done) {
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
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item =
        mockChrome.notifications.items['deviceNavigation:/device/path'];
    return item && item.message === 'REMOVABLE_DEVICE_NAVIGATION_MESSAGE' &&
        item.isClickable;
  });
  done(/*error=*/ false);
}

export async function testGoodDeviceWithAllowPlayStoreMessage(done) {
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
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const key = 'deviceNavigationAppAccess:/device/path';
    const item = mockChrome.notifications.items[key];
    return item && item.isClickable &&
        item.message ===
        'REMOVABLE_DEVICE_NAVIGATION_MESSAGE ' +
            'REMOVABLE_DEVICE_ALLOW_PLAY_STORE_ACCESS_MESSAGE';
  });

  done(/*error=*/ false);
}

export async function testGoodDeviceWithPlayStoreAppsHaveAccessMessage(done) {
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
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  // Since arcRemovableMediaAccessEnabled is true here, "Play Store apps have
  // access to ..." message is shown.
  await waitUntil(() => {
    const key = 'deviceNavigationAppAccess:/device/path';
    const item = mockChrome.notifications.items[key];
    return item && item.isClickable &&
        item.message ===
        'REMOVABLE_DEVICE_NAVIGATION_MESSAGE ' +
            'REMOVABLE_DEVICE_PLAY_STORE_APPS_HAVE_ACCESS_MESSAGE';
  });

  done(/*error=*/ false);
}

export function testRemovableMediaDeviceWithImportEnabled(done) {
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
    shouldNotify: true,
  });

  reportPromise(
      resolver.promise.then(event => {
        assertEquals('blabbity', event.volumeId);
      }),
      done);
}

export function testMtpMediaDeviceWithImportEnabled(done) {
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
    shouldNotify: true,
  });

  reportPromise(
      resolver.promise.then(event => {
        assertEquals('blabbity', event.volumeId);
      }),
      done);
}

export function testGoodDeviceNotNavigated() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: false,
  });

  assertEquals(0, Object.keys(mockChrome.notifications.items).length);
  assertFalse(mockChrome.notifications.resolver.settled);
}

export async function testGoodDeviceWithBadParent(done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'DEVICE_UNKNOWN: label';
  });
  done(/*error=*/ false);
}

export function testGoodDeviceWithBadParent_DuplicateMount(done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
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
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        assertEquals(
            'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
            notifications['deviceNavigation:/device/path'].message);
      }),
      done);
}

export function testUnsupportedDevice(done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertFalse(!!mockChrome.notifications.items['device:/device/path']);
        assertEquals(
            'DEVICE_UNSUPPORTED: label',
            mockChrome.notifications.items['deviceFail:/device/path'].message);
      }),
      done);
}

export async function testUnknownDevice(done) {
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
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item &&
        (item.message === 'DEVICE_UNKNOWN_DEFAULT_MESSAGE' &&
         item.buttons[0].title === 'DEVICE_UNKNOWN_BUTTON_LABEL');
  });

  assertFalse(!!mockChrome.notifications.items['device:/device/path']);
  done(/*error=*/ false);
}

export async function testUnknownReadonlyDevice(done) {
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
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'DEVICE_UNKNOWN_DEFAULT_MESSAGE' &&
        !item.buttons;
  });

  assertTrue(
      mockChrome.notifications.items['device:/device/path'] === undefined);
  done(/*error=*/ false);
}

export async function testUnsupportedWithUnknownParentReplacesNotification(
    done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'DEVICE_UNKNOWN: label';
  });

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'DEVICE_UNKNOWN: label';
  });
  done(/*error=*/ false);
}

export async function testMountPartialSuccess(done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const key = 'deviceNavigation:/device/path';
    const item = mockChrome.notifications.items[key];
    return item && item.message === 'REMOVABLE_DEVICE_NAVIGATION_MESSAGE';
  });

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const key = 'deviceFail:/device/path';
    const item = mockChrome.notifications.items[key];
    return item && item.message === 'MULTIPART_DEVICE_UNSUPPORTED: label';
  });
  done(/*error=*/ false);
}

export async function testUnknown(done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unknown',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const key = 'deviceFail:/device/path';
    const item = mockChrome.notifications.items[key];
    return item && item.message === 'DEVICE_UNKNOWN: label';
  });
  done(/*error=*/ false);
}

export function testNonASCIILabel(done) {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      // "RA (U+30E9) BE (U+30D9) RU (U+30EB)" in Katakana letters.
      deviceLabel: '\u30E9\u30D9\u30EB',
    },
    shouldNotify: true,
  });

  reportPromise(
      mockChrome.notifications.resolver.promise.then(notifications => {
        assertEquals(1, Object.keys(notifications).length);
        assertEquals(
            'DEVICE_UNKNOWN: \u30E9\u30D9\u30EB',
            notifications['deviceFail:/device/path'].message);
      }),
      done);
}

export async function testMultipleFail(done) {
  // The first parent error.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'DEVICE_UNKNOWN: label';
  });

  // The first child error that replaces the parent error.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'DEVICE_UNKNOWN: label';
  });

  // The second child error that turns to a multi-partition error.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'MULTIPART_DEVICE_UNSUPPORTED: label';
  });

  // The third child error that should be ignored because the error
  // message does not changed.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'MULTIPART_DEVICE_UNSUPPORTED: label';
  });
  done(/*error=*/ false);
}

export async function testDisabledDevice(done) {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'disabled', devicePath: '/device/path'});

  await waitUntil(() => {
    const item = mockChrome.notifications.items['deviceFail:/device/path'];
    return item && item.message === 'EXTERNAL_STORAGE_DISABLED_MESSAGE';
  });

  // Prepare for the second event: here we expect no notifications, thus
  // we are unable to wait until this happens. Instead, we synchronously invoke
  // the originalDoIfPrimaryContext and check that no notifications were
  // recorded.
  util.doIfPrimaryContext = async (fn) => {
    await originalDoIfPrimaryContext(fn);
    const keys = Object.keys(mockChrome.notifications.items);
    if (keys.length !== 0) {
      return reportError(`Unexpected keys ${keys}`, done);
    }
    done(/*error=*/ false);
    return Promise.resolve(true);
  };

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'removed', devicePath: '/device/path'});
}

export async function testFormatSucceeded(done) {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path', deviceLabel: 'label'});

  await waitUntil(() => {
    const item = progressCenter.getItemById('format:/device/path');
    return !!item && item.message === 'FORMAT_PROGRESS_MESSAGE: label';
  });

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_success',
    devicePath: '/device/path',
    deviceLabel: 'label',
  });

  await waitUntil(() => {
    const item = progressCenter.getItemById('format:/device/path');
    return !!item && item.message === 'FORMAT_SUCCESS_MESSAGE: label';
  });
  done(/*error=*/ false);
}

export async function testFormatFailed(done) {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path', deviceLabel: 'label'});

  await waitUntil(() => {
    const item = progressCenter.getItemById('format:/device/path');
    return !!item && item.message === 'FORMAT_PROGRESS_MESSAGE: label';
  });

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_fail', devicePath: '/device/path', deviceLabel: 'label'});

  await waitUntil(() => {
    const item = progressCenter.getItemById('format:/device/path');
    return !!item && item.message === 'FORMAT_FAILURE_MESSAGE: label';
  });
  done(/*error=*/ false);
}

export function testPartitionSucceeded(done) {
  util.doIfPrimaryContext = async (fn) => {
    await originalDoIfPrimaryContext(fn);
    const itemCount = progressCenter.getItemCount();
    if (itemCount !== 0) {
      return reportError(
          `Unexpected progress center item count got: ${itemCount} want: 0`,
          done);
    }
    util.doIfPrimaryContext = async (fn) => {
      await originalDoIfPrimaryContext(fn);
      const itemCount = progressCenter.getItemCount();
      if (itemCount !== 0) {
        return reportError(
            `Unexpected progress center item count got: ${itemCount} want: 0`,
            done);
      }
      done(/*error=*/ itemCount !== 0);
      return Promise.resolve(true);
    };
    // Second event: partition_success.
    mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
      type: 'partition_success',
      devicePath: '/device/path',
      deviceLabel: 'label',
    });
    return Promise.resolve(true);
  };
  // First event: partition_start.
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'partition_start',
    devicePath: '/device/path',
    deviceLabel: 'label',
  });
}

export function testPartitionFailed(done) {
  util.doIfPrimaryContext = async (fn) => {
    await originalDoIfPrimaryContext(fn);
    const itemCount = progressCenter.getItemCount();
    if (itemCount !== 0) {
      return reportError(
          `Unexpected progress center item count got: ${itemCount} want: 0`,
          done);
    }
    util.doIfPrimaryContext = async (fn) => {
      await originalDoIfPrimaryContext(fn);
      // Second event callback handled. Check the progress center.
      const itemCount = progressCenter.getItemCount();
      if (itemCount !== 1) {
        return reportError(
            `Unexpected progress center item count got:${itemCount}, want 1`,
            done);
      }
      const item = progressCenter.getItemById('partition:/device/path');
      if (item.message !== 'FORMAT_FAILURE_MESSAGE: label') {
        return reportError(`Unexpected item message "${item.message}"`, done);
      }
      done(/*error=*/ false);
      return Promise.resolve(true);
    };
    // Second event
    mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
      type: 'partition_fail',
      devicePath: '/device/path',
      deviceLabel: 'label',
    });
    return Promise.resolve(true);
  };

  // First event.
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'partition_start',
    devicePath: '/device/path',
    deviceLabel: 'label',
  });
}

export function testRenameSucceeded(done) {
  util.doIfPrimaryContext = async (fn) => {
    await originalDoIfPrimaryContext(fn);
    const keys = Object.keys(mockChrome.notifications.items);
    if (keys.length !== 0) {
      return reportError(`Unexpected keys ${keys}`, done);
    }
    util.doIfPrimaryContext = async (fn) => {
      await originalDoIfPrimaryContext(fn);
      const keys = Object.keys(mockChrome.notifications.items);
      done(/*error=*/ keys.length !== 0);
      return Promise.resolve(true);
    };
    mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
        {type: 'rename_success', devicePath: '/device/path'});
    return Promise.resolve(true);
  };
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_start', devicePath: '/device/path'});
}

export async function testRenameFailed(done) {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_start', devicePath: '/device/path'});

  // TODO(b/194246635): Fix this; async execution makes this check useless.
  assertEquals(0, Object.keys(mockChrome.notifications.items).length);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'rename_fail', devicePath: '/device/path'});

  await waitUntil(() => {
    const item = mockChrome.notifications.items['renameFail:/device/path'];
    return item &&
        item.message == 'RENAMING_OF_DEVICE_FINISHED_FAILURE_MESSAGE';
  });
  done(/*error=*/ false);
}

export async function testDeviceHardUnplugged(done) {
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'hard_unplugged', devicePath: '/device/path'});

  await waitUntil(() => {
    const item = mockChrome.notifications.items['hardUnplugged:/device/path'];
    return item && item.message === 'DEVICE_HARD_UNPLUGGED_MESSAGE';
  });
  done(/*error=*/ false);
}

export function testNotificationClicked(done) {
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
      done);
}

export function testMiscMessagesInIncognito(done) {
  setUpInIncognitoContext();

  util.doIfPrimaryContext = async (fn) => {
    await originalDoIfPrimaryContext(fn);
    done(0 !== Object.keys(mockChrome.notifications.items).length);
    return Promise.resolve(true);
  };
  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path', deviceLabel: 'label'});
}

export function testMountCompleteInIncognito(done) {
  setUpInIncognitoContext();

  util.doIfPrimaryContext = async (fn) => {
    await originalDoIfPrimaryContext(fn);
    done(0 !== Object.keys(mockChrome.notifications.items).length);
    return Promise.resolve(true);
  };

  mockChrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label',
    },
    shouldNotify: true,
  });
}

/**
 * Test that the device handler does not emit notifications when in a SWA window
 * if the isSwaEnabled flag is false.
 */
export function testIsSwaWindowTrueWithDisabledFlag() {
  setUpInIncognitoContext();
  window.isSWA = true;
  swaEnabledState = false;
  deviceHandler = new DeviceHandler(progressCenter);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path', deviceLabel: 'label'});
  assertEquals(0, progressCenter.getItemCount());

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_success',
    devicePath: '/device/path',
    deviceLabel: 'label',
  });
  assertEquals(0, progressCenter.getItemCount());
}

/**
 * Test that the device handler does not emit notifications when in a SWA window
 * if the isSwaEnabled flag is true.
 */
export function testSwaWindowWithEnabledFlag() {
  setUpInIncognitoContext();
  window.isSWA = true;
  swaEnabledState = false;
  deviceHandler = new DeviceHandler(progressCenter);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path', deviceLabel: 'label'});
  assertEquals(0, progressCenter.getItemCount());

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_success',
    devicePath: '/device/path',
    deviceLabel: 'label',
  });
  assertEquals(0, progressCenter.getItemCount());
}

/**
 * Test that the device handler does not emit notifications when not in a SWA
 * window if the isSwaEnabled flag is true.
 */
export function testNoSwaWindowWithEnabledFlag() {
  setUpInIncognitoContext();
  window.isSWA = false;
  swaEnabledState = true;
  deviceHandler = new DeviceHandler(progressCenter);

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch(
      {type: 'format_start', devicePath: '/device/path', deviceLabel: 'label'});
  assertEquals(0, progressCenter.getItemCount());

  mockChrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_success',
    devicePath: '/device/path',
    deviceLabel: 'label',
  });
  assertEquals(0, progressCenter.getItemCount());
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
        },
      },
      onMountCompleted: {
        dispatch: null,
        addListener: function(listener) {
          mockChrome.fileManagerPrivate.onMountCompleted.dispatch = listener;
        },
      },
      getProfiles: function(callback) {
        callback([{profileId: 'userid@xyz.domain.org'}]);
      },
      getPreferences: function(callback) {
        callback({
          arcEnabled: mockChrome.fileManagerPrivate.arcEnabledPref,
          arcRemovableMediaAccessEnabled:
              mockChrome.fileManagerPrivate.arcRemovableMediaAccessEnabledPref,
        });
      },
      arcEnabledPref: false,
      arcRemovableMediaAccessEnabledPref: true,
    },
    i18n: {
      getUILanguage: function() {
        return 'en-US';
      },
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
        },
      },
      onClicked: {
        dispatch: null,
        addListener: function(listener) {
          mockChrome.notifications.onClicked.dispatch = listener;
        },
      },
      getAll: function(callback) {
        callback([]);
      },
    },
    runtime: {
      getURL: function(path) {
        return path;
      },
      onStartup: {addListener: function() {}},
    },
  };

  installMockChrome(mockChrome);
}
