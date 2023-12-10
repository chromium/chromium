// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import type {MetadataRequest} from './metadata_request.js';

/**
 * Metadata provider for FileEntry#getMetadata.
 * @final
 */
export class FileSystemMetadataProvider extends MetadataProvider {
  static readonly PROPERTY_NAMES =
      ['modificationTime', 'size', 'present', 'availableOffline'];

  constructor() {
    super(FileSystemMetadataProvider.PROPERTY_NAMES);
  }

  override get(requests: MetadataRequest[]): Promise<MetadataItem[]> {
    if (!requests.length) {
      return Promise.resolve([]);
    }
    return Promise.all(requests.map(request => {
      return new Promise<Metadata>((fulfill, reject) => {
               request.entry.getMetadata(fulfill, reject);
             })
          .then(
              result => {
                const item = new MetadataItem();
                item.modificationTime = result.modificationTime;
                item.size = request.entry.isDirectory ? -1 : result.size;
                item.present = true;
                item.availableOffline = true;
                return item;
              },
              error => {
                // Can't use console.error because some tests hit this line and
                // console.error causes them to fail because of JSErrorCount.
                // This error is an acceptable condition.
                console.warn(`Cannot get metadata for '${
                    request.entry.toURL()}': ${error}`);
                return new MetadataItem();
              });
    }));
  }
}
