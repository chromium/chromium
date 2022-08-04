// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.fileManagerPrivate API */
// TODO(crbug.com/1203307)= Auto-generate this file.

export {};

declare global {
  export namespace chrome {
    export namespace fileManagerPrivate {
      export enum SourceRestriction {
        ANY_SOURCE = 'any_source',
        NATIVE_SOURCE = 'native_source',
      }

      export enum RecentFileType {
        ALL = 'all',
        AUDIO = 'audio',
        IMAGE = 'image',
        VIDEO = 'video',
        DOCUMENT = 'document',
      }

      export enum DriveConnectionStateType {
        OFFLINE = 'OFFLINE',
        METERED = 'METERED',
        ONLINE = 'ONLINE',
      }
      export enum DriveOfflineReason {
        NOT_READY = 'NOT_READY',
        NO_NETWORK = 'NO_NETWORK',
        NO_SERVICE = 'NO_SERVICE',
      }

      export enum VmType {
        TERMINA = 'termina',
        PLUGIN_VM = 'plugin_vm',
        BOREALIS = 'borealis',
        BRUSCHETTA = 'bruschetta',
        ARCVM = 'arcvm',
      }

      export interface IconSet {
        icon16x16Url?: string;
        icon32x32Url?: string;
      }

      export interface DriveConnectionState {
        type: chrome.fileManagerPrivate.DriveConnectionStateType;
        reason?: chrome.fileManagerPrivate.DriveOfflineReason;
        hasCellularNetworkAccess: boolean;
        canPinHostedFiles: boolean;
      }
    }
  }
}
