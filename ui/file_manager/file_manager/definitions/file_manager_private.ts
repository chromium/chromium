// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable */

/**
 * @fileoverview This file is used for chrome.fileManagerPrivate definitions
 * that will override the Closure markup. Closure defines optional properties
 * with "type|undefined" which TypeScript interprets as not optional but
 * required with an undefined type. To mark those as optional to TypeScript,
 * override the definitions here, using the `Partial` directive.
 *
 *  This file is currently has a .ts extension and NOT .d.ts to ensure it can
 *  co-exist with the Closure markup. When the TypeScript migration has
 *  completed, the whole chrome.fileManagerPrivate.* should be defined in here
 *  and this file can be renamed to .d.ts.
 */

declare namespace chrome {
  export namespace fileManagerPrivate {
    type PreferencesChange = {
      driveSyncEnabledOnMeteredNetwork: boolean,
      arcEnabled: boolean,
      arcRemovableMediaAccessEnabled: boolean,
      folderShortcuts: string[],
      driveFsBulkPinningEnabled: boolean,
    }

    export function setPreferences(change: Partial<PreferencesChange>): void;

    enum DriveConnectionStateType {
      OFFLINE = 'OFFLINE',
      METERED = 'METERED',
      ONLINE = 'ONLINE',
    }

    enum DriveOfflineReason {
      NOT_READY = 'NOT_READY',
      NO_NETWORK = 'NO_NETWORK',
      NO_SERVICE = 'NO_SERVICE',
    }

    type DriveConnectionState = {
      type: chrome.fileManagerPrivate.DriveConnectionStateType,
      reason?: chrome.fileManagerPrivate.DriveOfflineReason,
    }

    export type GetDriveConnectionStateCallback =
        (state: DriveConnectionState) => void;

    export function getDriveConnectionState(
        callback: GetDriveConnectionStateCallback): void;

    type IOTaskParams = {
      destinationFolder?: DirectoryEntry,
      password?: string,
      showNotification?: boolean,
    }
  }
}

interface ChromeWindow extends Window {
  chrome: typeof chrome;
}
