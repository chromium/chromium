// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class DirectoryChangeEvent extends Event {
  constructor() {
    /** @type {DirectoryEntry} */
    this.previousDirEntry;

    /** @type {DirectoryEntry|FakeEntry} */
    this.newDirEntry;

    /** @type {boolean} */
    this.volumeChanged;
  }
}
