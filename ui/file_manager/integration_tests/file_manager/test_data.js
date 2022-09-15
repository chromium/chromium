// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, TestEntryInfo} from '../test_util.js';

/**
 * Extension ID of the Files app.
 * @type {string}
 * @const
 */
export const FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * App ID of Files app SWA.
 * @type {string}
 * @const
 */
export const FILE_MANAGER_SWA_APP_ID = 'fkiggjmkendpmbegkagpmagjepfkpmeb';

/**
 * Base URL of Files app SWA.
 * @const {string}
 */
export const FILE_SWA_BASE_URL = 'chrome://file-manager/';

/**
 * Basic entry set for the local volume.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
export const BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN = BASIC_LOCAL_ENTRY_SET.concat([
  ENTRIES.hiddenFile,
  ENTRIES.dotTrash,
]);

/**
 * Basic entry set for the drive volume that only includes read-write entries
 * (no read-only or similar entries).
 *
 * @type {!Array<TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
export const BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN = BASIC_DRIVE_ENTRY_SET.concat([
  ENTRIES.hiddenFile,
]);

/**
 * Expected files shown in Drive with Google Docs disabled
 *
 * @type {!Array<!TestEntryInfo>}
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
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
export const BASIC_CROSTINI_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
];

/**
 * Basic entry set for the local volume with a ZIP archive.
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
export const BASIC_ZIP_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.zipArchive,
];

/**
 * More complex entry set for the local volume with multiple ZIP archives.
 * @type {!Array<!TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const NESTED_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC,
];

/**
 * Expected list of preset entries in fake test volumes. This should be in sync
 * with FakeTestVolume::PrepareTestEntries in the test harness.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const BASIC_FAKE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.directoryA,
];

/**
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const RECENT_ENTRY_SET = [
  ENTRIES.desktop,
  ENTRIES.beautiful,
];

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const OFFLINE_ENTRY_SET = [
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 *
 * @type {!Array<TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<TestEntryInfo>}
 * @const
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
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const COMPUTERS_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.computerA,
  ENTRIES.computerAFile,
  ENTRIES.computerAdirectoryA,
];

/**
 * Basic entry set for the android volume.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const BASIC_ANDROID_ENTRY_SET = [
  ENTRIES.directoryDocuments,
  ENTRIES.directoryMovies,
  ENTRIES.directoryMusic,
  ENTRIES.directoryPictures,
];

/**
 * Expected files shown in Android with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
export const BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN =
    BASIC_ANDROID_ENTRY_SET.concat([
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.directoryA,
    ]);

/**
 * Entry set for modified times.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
export const MODIFIED_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.photos,
  ENTRIES.invalidLastModifiedDate,
];