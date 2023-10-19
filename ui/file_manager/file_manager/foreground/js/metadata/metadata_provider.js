// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {MetadataItem} from './metadata_item.js';
import {MetadataRequest} from './metadata_request.js';

/**
 * @abstract
 */
export class MetadataProvider {
  /**
   * @param {!Array<string>} validPropertyNames
   */
  constructor(validPropertyNames) {
    /**
     * Set of valid property names. Key is the name of property and value is
     * always true.
     * @private @const @type {!Record<string, boolean>}
     */
    this.validPropertyNames_ = {};
    for (let i = 0; i < validPropertyNames.length; i++) {
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      this.validPropertyNames_[validPropertyNames[i]] = true;
    }
  }

  // @ts-ignore: error TS7006: Parameter 'names' implicitly has an 'any' type.
  checkPropertyNames(names) {
    // Check if the property name is correct or not.
    for (let i = 0; i < names.length; i++) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'any' can't be used to index type '{}'.
      assert(this.validPropertyNames_[names[i]], names[i]);
    }
  }

  /**
   * Obtains the metadata for the request.
   * @abstract
   * @param {!Array<!MetadataRequest>} requests
   * @return {!Promise<!Array<!MetadataItem>>} Promise with obtained metadata.
   *     It should not return rejected promise. Instead it should return
   *     undefined property for property error, and should return empty
   *     MetadataItem for entry error.
   */
  // @ts-ignore: error TS6133: 'requests' is declared but its value is never
  // read.
  get(requests) {
    return Promise.resolve([]);
  }
}
