// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../common/js/util.js';

export class EntriesChangedEvent extends Event {
  /** @param {string} eventName */
  constructor(eventName) {
    super(eventName);

    /** @type {util.EntryChangedKind} */
    this.kind;

    /** @type {Array<!Entry>} */
    this.entries;
  }
}
