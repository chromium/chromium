// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.mimeHandlerPrivate API. */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace mimeHandlerPrivate {
      export interface StreamInfo {
        mimeType: string;
        originalUrl: string;
        streamUrl: string;
        tabId: number;
        responseHeaders: Object;
        embedded: boolean;
      }

      export interface PdfPluginAttributes {
        backgroundColor: number;
        allowJavascript: boolean;
      }

      export function getStreamInfo(callback: (info: StreamInfo) => void): void;
      export function setPdfPluginAttributes(attributesForLoading:
                                                 PdfPluginAttributes): void;
      export function setShowBeforeUnloadDialog(
          showDialog: boolean, callback?: () => void): void;

      export const onSave: ChromeEvent<(url: string) => void>;
    }
  }
}
