// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.indigoPrivate API. */

export {};

declare global {
  export namespace chrome {
    export namespace indigoPrivate {
      export interface ImageData {
        webpBytes: ArrayBuffer;
      }
      export function readyToRender(): Promise<void>;
      export function getOriginalImage(): Promise<ImageData>;
    }
  }
}
