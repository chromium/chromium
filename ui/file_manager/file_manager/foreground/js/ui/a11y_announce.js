// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class A11yAnnounce {
  /**
   * @param {string} text Text to be announced by screen reader, which should be
   * already translated.
   */
  // @ts-ignore: error TS6133: 'text' is declared but its value is never read.
  speakA11yMessage(text) {}
}
