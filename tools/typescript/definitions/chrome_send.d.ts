// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.send API */

declare namespace chrome {
  function send(msg: string, params?: Array<any>): void;
  function getVariableValue(name: string): string;
}
