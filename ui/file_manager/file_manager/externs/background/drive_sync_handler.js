// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handler of the background page for the Drive sync events. Implementations
 * of this interface must @extends {cr.EventTarget}.
 *
 * @interface
 */
export class DriveSyncHandler extends EventTarget {
  /**
   * Returns the completed event name.
   * @return {string}
   */
  getCompletedEventName() {}

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {}

  /**
   * @param {Object} model
   */
  set metadataModel(model) {}
}
