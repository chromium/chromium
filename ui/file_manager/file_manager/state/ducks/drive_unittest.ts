// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {setUpFileManagerOnWindow} from '../for_tests.js';
import {getEmptyState, getStore, type Store} from '../store.js';

import {updateDriveConnectionStatus} from './drive.js';

let store: Store;

export function setUp() {
  setUpFileManagerOnWindow();
  store = getStore();
  store.init(getEmptyState());
}

export function testUpdateDriveConnection() {
  // Connection type if properly added to the store.
  store.dispatch(updateDriveConnectionStatus({
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
  }));
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
      store.getState().drive.connectionType);

  // ONLINE connection type with offline reason has the reason ignored.
  store.dispatch(updateDriveConnectionStatus({
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
    reason: chrome.fileManagerPrivate.DriveOfflineReason.NOT_READY,
  }));
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
      store.getState().drive.connectionType);
  assertEquals(undefined, store.getState().drive.offlineReason);

  // METERED connection type with offline reason has the reason ignored.
  store.dispatch(updateDriveConnectionStatus({
    type: chrome.fileManagerPrivate.DriveConnectionStateType.METERED,
    reason: chrome.fileManagerPrivate.DriveOfflineReason.NOT_READY,
  }));
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.METERED,
      store.getState().drive.connectionType);
  assertEquals(undefined, store.getState().drive.offlineReason);

  // OFFLINE connection type with offline reason gets updated in the store.
  store.dispatch(updateDriveConnectionStatus({
    type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
    reason: chrome.fileManagerPrivate.DriveOfflineReason.NOT_READY,
  }));
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
      store.getState().drive.connectionType);
  assertEquals(
      chrome.fileManagerPrivate.DriveOfflineReason.NOT_READY,
      store.getState().drive.offlineReason);
}
