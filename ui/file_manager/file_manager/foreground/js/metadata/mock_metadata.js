// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-ignore: error TS6133: 'MetadataModel' is declared but its value is never
// read.
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
     * @public @const @type {Object}
     */
    this.properties = initial_properties;

    /**
     * Per entry properties, which can be set by a test.
     * @private @const @type {Map<string, Object>}
     */
    this.propertiesMap_ = new Map();
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any' type.
  get(entries) {
    return Promise.resolve(this.getCache(entries));
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any' type.
  getCache(entries) {
    return entries.map(
        // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any'
        // type.
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
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'MockMetadataModel' does not
  // extend another class.
  notifyEntriesChanged() {}
}
