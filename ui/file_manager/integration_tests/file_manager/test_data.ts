// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES} from '../test_util.js';

/**
 * Extension ID of the Files app.
 */
export const FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * App ID of Files app SWA.
 */
export const FILE_MANAGER_SWA_APP_ID = 'fkiggjmkendpmbegkagpmagjepfkpmeb';

/**
 * Base URL of Files app SWA.
 */
export const FILE_SWA_BASE_URL = 'chrome://file-manager/';

/**
 * Basic entry set for the local volume.
 */
export const BASIC_LOCAL_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
];

/**
 * Expected files shown in Downloads with hidden enabled
 */
export const BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN = BASIC_LOCAL_ENTRY_SET.concat([
  ENTRIES.hiddenFile,
  ENTRIES.dotTrash,
]);

/**
 * Basic entry set for the drive volume that only includes read-write entries
 * (no read-only or similar entries).
 */
export const BASIC_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Expected files shown in Drive with hidden enabled
 */
export const BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN = BASIC_DRIVE_ENTRY_SET.concat([
  ENTRIES.hiddenFile,
]);

/**
 * Expected files shown in Drive with Google Docs disabled
 */
export const BASIC_DRIVE_ENTRY_SET_WITHOUT_GDOCS = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testSharedFile,
];

/**
 * Basic entry set for the local crostini volume.
 */
export const BASIC_CROSTINI_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
];

/**
 * Basic entry set for the local volume with a ZIP archive.
 */
export const BASIC_ZIP_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.zipArchive,
];

/**
 * More complex entry set for the local volume with multiple ZIP archives.
 */
export const COMPLEX_ZIP_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.zipArchive,
  ENTRIES.zipSJISArchive,
];

/**
 * More complex entry set for Drive that includes entries with varying
 * permissions (such as read-only entries).
 */
export const COMPLEX_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.photos,
  ENTRIES.readOnlyFolder,
  ENTRIES.readOnlyDocument,
  ENTRIES.readOnlyStrictDocument,
  ENTRIES.readOnlyFile,
];

/**
 * More complex entry set for DocumentsProvider that includes entries with
 * arying permissions (such as read-only entries).
 */
export const COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.photos,
  ENTRIES.readOnlyFolder,
  ENTRIES.readOnlyFile,
  ENTRIES.deletableFile,
  ENTRIES.renamableFile,
];

/**
 * Nested entry set (directories inside each other).
 */
export const NESTED_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC,
];

/**
 * Expected list of preset entries in fake test volumes. This should be in sync
 * with FakeTestVolume::PrepareTestEntries in the test harness.
 */
export const BASIC_FAKE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.directoryA,
];

/**
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 */
export const RECENT_ENTRY_SET = [
  ENTRIES.desktop,
  ENTRIES.beautiful,
];

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 */
export const OFFLINE_ENTRY_SET = [
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 */
export const SHARED_WITH_ME_ENTRY_SET = [
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Entry set for Drive that includes team drives of various permissions and
 * nested files with various permissions.
 *
 * TODO(sashab): Add support for capabilities of Shared Drive roots.
 */
export const SHARED_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.teamDriveA,
  ENTRIES.teamDriveAFile,
  ENTRIES.teamDriveADirectory,
  ENTRIES.teamDriveAHostedFile,
  ENTRIES.teamDriveB,
  ENTRIES.teamDriveBFile,
  ENTRIES.teamDriveBDirectory,
];

/**
 * Entry set for Drive that includes Computers, including nested computers with
 * files and nested "USB and External Devices" with nested devices.
 */
export const COMPUTERS_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.computerA,
  ENTRIES.computerAFile,
  ENTRIES.computerAdirectoryA,
];

/**
 * Basic entry set for the android volume.
 */
export const BASIC_ANDROID_ENTRY_SET = [
  ENTRIES.directoryDocuments,
  ENTRIES.directoryMovies,
  ENTRIES.directoryMusic,
  ENTRIES.directoryPictures,
];

/**
 * Expected files shown in Android with hidden enabled
 */
export const BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN =
    BASIC_ANDROID_ENTRY_SET.concat([
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.directoryA,
    ]);

/**
 * Entry set for modified times.
 */
export const MODIFIED_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.photos,
  ENTRIES.invalidLastModifiedDate,
];


/**
 * Fake task.
 */
export class FakeTask {
  driveApp = false;
  iconUrl = 'chrome://theme/IDR_DEFAULT_FAVICON';  // Dummy icon

  /**
   * @param isDefault Whether the task is default or not.
   * @param descriptor Task descriptor.
   * @param title Title of the task.
   * @param isGenericFileHandler Whether the task is a generic file handler.
   * @param isDlpBlocked Whether the task is blocked by DLP.
   */
  constructor(
      public isDefault: boolean,
      public descriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
      public title?: string, public isGenericFileHandler: boolean = false,
      public isDlpBlocked: boolean = false) {
    Object.freeze(this);
  }
}

/**
 * Fake tasks for a local volume.
 */
export const DOWNLOADS_FAKE_TASKS = [
  new FakeTask(
      true,
      {appId: 'dummytaskid', taskType: 'fake-type', actionId: 'open-with'},
      'DummyTask1'),
  new FakeTask(
      false,
      {appId: 'dummytaskid-2', taskType: 'fake-type', actionId: 'open-with'},
      'DummyTask2'),
];
