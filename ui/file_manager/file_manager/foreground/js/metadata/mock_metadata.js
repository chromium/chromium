// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataModel} from './metadata_model.js';

/**
 * Returns a mock of metadata model.
 *
 * @extends {MetadataModel}
 * @final
 */
export class MockMetadataModel {
  /** @param {Object} initial_properties */
  constructor(initial_properties) {
    /**
     * Dummy properties, which can be overwritten by a test.
     * @public @const {Object}
     */
    this.properties = initial_properties;

    /**
     * Per entry properties, which can be set by a test.
     * @private @const {Map<string, Object>}
     */
    this.propertiesMap_ = new Map();
  }

  /** @override */
  get(entries) {
    return Promise.resolve(this.getCache(entries));
  }

  /** @override */
  getCache(entries) {
    return entries.map(
        entry => this.propertiesMap_.has(entry.toURL()) ?
            this.propertiesMap_.get(entry.toURL()) :
            this.properties);
  }

  /**
   * @param {Entry} entry
   * @param {Object} properties
   */
  set(entry, properties) {
    this.propertiesMap_.set(entry.toURL(), properties);
  }

  /** @override */
  notifyEntriesChanged() {}
}
