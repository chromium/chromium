// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class EntriesChangedEvent extends Event {
  constructor() {
    /** @type {util.EntryChangedKind} */
    this.kind;

    /** @type {Array<!Entry>} */
    this.entries;
  }
}
