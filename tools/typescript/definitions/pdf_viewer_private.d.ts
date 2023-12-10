// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.pdfViewerPrivate API. */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace pdfViewerPrivate {
      // `mimeType` and `responseHeaders` are unused fields, but they are
      // necessary to be able to cast to chrome.mimeHandlerPrivate.StreamInfo.
      // TODO(crbug.com/1445746): Remove `mimeType` and `responseHeaders` after
      // PDF viewer no longer uses chrome.mimeHandlerPrivate.
      export interface StreamInfo {
        mimeType: string;
        originalUrl: string;
        streamUrl: string;
        tabId: number;
        responseHeaders: Record<string, string>;
        embedded: boolean;
      }

      export interface PdfPluginAttributes {
        backgroundColor: number;
        allowJavascript: boolean;
      }

      export function getStreamInfo(callback: (info: StreamInfo) => void): void;
      export function isAllowedLocalFileAccess(
          url: string, callback: (isAllowed: boolean) => void): void;
      export function isPdfOcrAlwaysActive(
          callback: (isAlwaysActive: boolean) => void): void;
      export function setPdfOcrPref(
          isAlwaysActive: boolean, callback: (isSet: boolean) => void): void;
      export function setPdfPluginAttributes(attributes: PdfPluginAttributes):
          void;

      type PdfOcrPrefCallback = ((isPdfOcrAlwaysActive: boolean) => void)|null;
      export const onPdfOcrPrefChanged: ChromeEvent<PdfOcrPrefCallback>;
    }
  }
}
