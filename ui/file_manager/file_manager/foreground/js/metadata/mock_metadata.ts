// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';

import type {MetadataItem} from './metadata_item.js';
import {type MetadataKey} from './metadata_item.js';
import {MetadataModel} from './metadata_model.js';
import {MetadataProvider} from './metadata_provider.js';
import type {MetadataRequest} from './metadata_request.js';

/**
 * Mock metadata provider that doesn't actually do anything just so
 * MockMetadataModel has an object it can pass to MetadataModel's constructor.
 */
class MockMetadataProvider extends MetadataProvider {
  override get(_requests: MetadataRequest[]): Promise<MetadataItem[]> {
    assertNotReached('Method not implemented.');
  }
}

/**
 * Returns a mock of metadata model.
 */
export class MockMetadataModel extends MetadataModel {
  /**
   * Per entry properties, which can be set by a test.
   */
  private propertiesMap_: Map<string, MetadataItem> = new Map();

  /**
   * @param properties Default properties, which can be overwritten by a test.
   */
  constructor(public properties: MetadataItem) {
    super(new MockMetadataProvider([]));
  }

  override get(entries: Array<Entry|FilesAppEntry>, _names: MetadataKey[]) {
    return Promise.resolve(this.getCache(entries, []));
  }

  override getCache(
      entries: Array<Entry|FilesAppEntry>, _names: MetadataKey[] = []) {
    return entries.map(
        entry => this.propertiesMap_.get(entry.toURL()) || this.properties);
  }

  set(entry: Entry, properties: MetadataItem) {
    this.propertiesMap_.set(entry.toURL(), properties);
  }

  override notifyEntriesChanged() {}
}
