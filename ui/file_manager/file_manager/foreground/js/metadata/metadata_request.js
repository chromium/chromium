// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MetadataRequest {
  /**
   * @param {!Entry} entry Entry
   * @param {!Array<string>} names Property name list to be requested.
   */
  constructor(entry, names) {
    /**
     * @public {!Entry}
     * @const
     */
    this.entry = entry;

    /**
     * @public {!Array<string>}
     * @const
     */
    this.names = names;
  }
}
