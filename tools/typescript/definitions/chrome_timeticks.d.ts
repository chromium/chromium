// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.timeticks API */

declare namespace chrome {
  export namespace timeTicks {
    function nowInMicroseconds(): bigint;
  }
}