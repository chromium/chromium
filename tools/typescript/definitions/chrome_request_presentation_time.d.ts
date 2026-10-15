// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.requestPresentationTime API */

declare namespace chrome {
  function requestPresentationTime(): Promise<bigint|null>;
}
