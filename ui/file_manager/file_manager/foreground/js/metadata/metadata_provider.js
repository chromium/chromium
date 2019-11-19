// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @abstract
 */
class MetadataProvider {
  /**
   * @param {!Array<string>} validPropertyNames
   */
  constructor(validPropertyNames) {
    /**
     * Set of valid property names. Key is the name of property and value is
     * always true.
     * @private @const {!Object<boolean>}
     */
    this.validPropertyNames_ = {};
    for (let i = 0; i < validPropertyNames.length; i++) {
      this.validPropertyNames_[validPropertyNames[i]] = true;
    }
  }

  checkPropertyNames(names) {
    // Check if the property name is correct or not.
    for (let i = 0; i < names.length; i++) {
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
  get(requests) {}
}
