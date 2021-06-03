// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.bookmarkManagerPrivate API. */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace management {
    export interface UninstallOptions {
      showConfirmDialog?: boolean;
    }

    export function uninstall(
        id: string, options?: UninstallOptions, callback?: () => void): void;
    export function setEnabled(
        id: string, enabled: boolean, callback?: () => void): void;
  }
}
