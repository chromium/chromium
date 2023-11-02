// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.send API */

declare namespace chrome {
  function send(msg: string, params?: any[]): void;
  function getVariableValue(name: string): string;
}
