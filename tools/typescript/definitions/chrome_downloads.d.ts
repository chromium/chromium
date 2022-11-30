// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.downloads API */

declare namespace chrome {
  export namespace downloads {
    function getFileIcon(downloadId: number, resolve: (data: string) => void):
        void;
  }
}
