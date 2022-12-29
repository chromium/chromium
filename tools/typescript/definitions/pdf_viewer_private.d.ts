// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.pdfViewerPrivate API. */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace pdfViewerPrivate {

    export function isAllowedLocalFileAccess(
        url: string, callback: (isAllowed: boolean) => void): void;
  }
}
