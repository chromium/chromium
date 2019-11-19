// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a mock of metadata model.
 *
 * @extends {MetadataModel}
 * @final
 */
class MockMetadataModel {
  /** @param {Object} initial_properties */
  constructor(initial_properties) {
    /**
     * Dummy properties, which can be overwritten by a test.
     * @public @const {Object}
     */
    this.properties = initial_properties;
  }

  /** @override */
  get() {
    return Promise.resolve([this.properties]);
  }

  /** @override */
  getCache() {
    return [this.properties];
  }

  /** @override */
  notifyEntriesChanged() {}
}
