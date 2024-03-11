// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.storage API
 * Generated from: extensions/common/api/storage.json
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/storage.json -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace storage {

      // eslint-disable-next-line @typescript-eslint/naming-convention
      interface sync_StorageArea extends StorageArea {
        readonly QUOTA_BYTES: number;
        readonly QUOTA_BYTES_PER_ITEM: number;
        readonly MAX_ITEMS: number;
        readonly MAX_WRITE_OPERATIONS_PER_HOUR: number;
        readonly MAX_WRITE_OPERATIONS_PER_MINUTE: number;
        readonly MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE: number;
      }
      export const sync: sync_StorageArea;

      // eslint-disable-next-line @typescript-eslint/naming-convention
      interface local_StorageArea extends StorageArea {
        readonly QUOTA_BYTES: number;
      }
      export const local: local_StorageArea;

      export const managed: StorageArea;

      // eslint-disable-next-line @typescript-eslint/naming-convention
      interface session_StorageArea extends StorageArea {
        readonly QUOTA_BYTES: number;
      }
      export const session: session_StorageArea;

      export enum AccessLevel {
        TRUSTED_CONTEXTS = 'TRUSTED_CONTEXTS',
        TRUSTED_AND_UNTRUSTED_CONTEXTS = 'TRUSTED_AND_UNTRUSTED_CONTEXTS',
      }

      export interface StorageChange {
        oldValue?: any;
        newValue?: any;
      }

      export interface StorageArea {
        get(keys?: string|string[]|{
          [key: string]: any,
        }): Promise<{
          [key: string]: any,
        }>;
        getBytesInUse(keys?: string|string[]): Promise<number>;
        set(items: {
          [key: string]: any,
        }): Promise<void>;
        remove(keys: string|string[]): Promise<void>;
        clear(): Promise<void>;
        setAccessLevel(accessOptions: {
          accessLevel: AccessLevel,
        }): Promise<void>;
        onChanged: ChromeEvent<(changes: {
                                 [key: string]: StorageChange,
                               }) => void>;
      }

      export const onChanged: ChromeEvent<
          (changes: {
            [key: string]: StorageChange,
          },
           areaName: string) => void>;
    }
  }
}
