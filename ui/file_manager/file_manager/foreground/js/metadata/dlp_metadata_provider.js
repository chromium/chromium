// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDlpMetadata} from '../../../common/js/api.js';
import {util} from '../../../common/js/util.js';

import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';

/**
 * Metadata provider for FileEntry#getMetadata.
 * Returns Data Leak Prevention (DLP) status of the file, such as whether the
 * file is restricted or not.
 * @final
 */
export class DlpMetadataProvider extends MetadataProvider {
  constructor() {
    super(DlpMetadataProvider.PROPERTY_NAMES);
  }

  /** @override */
  async get(requests) {
    if (!util.isDlpEnabled()) {
      return requests.map(() => new MetadataItem());
    }

    if (!requests.length) {
      return [];
    }

    const entries = requests.map(_request => {
      return _request.entry;
    });

    try {
      const dlpMetadataList = await getDlpMetadata(entries);
      if (dlpMetadataList.length != entries.length) {
        console.warn(`Requested ${entries.length} entries, got ${
            dlpMetadataList.length}.`);
        return requests.map(() => {
          return new MetadataItem();
        });
      }

      const results = [];
      for (let i = 0; i < dlpMetadataList.length; i++) {
        const item = new MetadataItem();
        item.isDlpRestricted = dlpMetadataList[i].isDlpRestricted;
        item.sourceUrl = dlpMetadataList[i].sourceUrl;
        results.push(item);
      }
      return results;
    } catch (error) {
      console.warn(error);
      return requests.map(() => {
        return new MetadataItem();
      });
    }
  }
}

/** @const {!Array<string>} */
DlpMetadataProvider.PROPERTY_NAMES = [
  // TODO(crbug.com/1329770): Consider using an enum for this property.
  'isDlpRestricted',
  'sourceUrl',
];
