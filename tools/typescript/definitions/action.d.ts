// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.action API */

declare namespace chrome {
  export namespace action {
    export interface SetTitleDetails {
      title: string;
      tabId?: number;
    }

    export function setTitle(details: SetTitleDetails): Promise<void>;
  }
}
