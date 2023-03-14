// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.pdfViewerPrivate API. */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace pdfViewerPrivate {

      export function isAllowedLocalFileAccess(
          url: string, callback: (isAllowed: boolean) => void): void;
      export function isPdfOcrAlwaysActive(
          callback: (isAlwaysActive: boolean) => void): void;
      export function setPdfOcrPref(
          isAlwaysActive: boolean, callback: (isSet: boolean) => void): void;

      type PdfOcrPrefCallback = ((isPdfOcrAlwaysActive: boolean) => void)|null;
      export const onPdfOcrPrefChanged: ChromeEvent<PdfOcrPrefCallback>;
    }
  }
}
