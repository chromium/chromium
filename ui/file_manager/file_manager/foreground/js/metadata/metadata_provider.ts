// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {MetadataItem} from './metadata_item.js';
import {type MetadataKey} from './metadata_item.js';
import type {MetadataRequest} from './metadata_request.js';

export abstract class MetadataProvider {
  private readonly validPropertyNames_: Set<string>;

  constructor(validPropertyNames: string[]) {
    this.validPropertyNames_ = new Set(validPropertyNames);
  }

  checkPropertyNames(names: readonly string[]): asserts names is MetadataKey[] {
    // Check if the property name is correct or not.
    for (const propertyName of names) {
      assert(this.validPropertyNames_.has(propertyName), propertyName);
    }
  }

  /**
   * Obtains the metadata for the request.
   * @return Promise with obtained metadata.
   *     It should not return rejected promise. Instead it should return
   *     undefined property for property error, and should return empty
   *     MetadataItem for entry error.
   */
  abstract get(requests: MetadataRequest[]): Promise<MetadataItem[]>;
}
