// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.storage API. */
// TODO(crbug.com/1203307): Auto-generate this file
// from extensions/common/api/storage.json.

declare namespace chrome {
  export namespace storage {
    export interface StorageArea {
      get(keys?: string|string[]): Promise<Object[]>;
      set(items: {[key: string]: any}): Promise<void>;
    }

    export const session: StorageArea;
  }
}
