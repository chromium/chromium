// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface that Files app uses from <paper-ripple>.
 */
class PaperRipple extends HTMLElement {
  simulatedRipple() {}

  /**
   * @param {Event=} event
   */
  downAction(event) {}

  /**
   * @param {Event=} event
   */
  upAction(event) {}
}
