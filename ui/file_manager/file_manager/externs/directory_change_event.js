// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {externsValidation} this file is used as externs and also
 * as JS module, Closure fails to compile as externs.
 */

import {FakeEntry} from './files_app_entry_interfaces.js';

export class DirectoryChangeEvent extends Event {
  /** @param {string} eventName */
  constructor(eventName) {
    super(eventName);

    /** @type {DirectoryEntry} */
    this.previousDirEntry;

    /** @type {DirectoryEntry|FakeEntry} */
    this.newDirEntry;

    /** @type {boolean} */
    this.volumeChanged;
  }
}
