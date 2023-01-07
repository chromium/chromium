// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.quickUnlockPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace quickUnlockPrivate {
    export interface TokenInfo {
      token: string;
      lifetimeSeconds: number;
    }

    export function getAuthToken(
        accountPassword: string, onComplete: (info: TokenInfo) => void): void;
  }
}
