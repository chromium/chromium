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

    // Filter out fake entries before fetching the metadata.
    const entries =
        requests.map(r => r.entry).filter(e => !util.isFakeEntry(e));

    if (!entries.length) {
      return requests.map(() => new MetadataItem());
    }

    try {
      const dlpMetadataList = await getDlpMetadata(entries);
      if (dlpMetadataList.length != entries.length) {
        console.warn(`Requested ${entries.length} entries, got ${
            dlpMetadataList.length}.`);
        return requests.map(() => new MetadataItem());
      }

      const results = [];
      let j = 0;
      for (let i = 0; i < requests.length; i++) {
        const item = new MetadataItem();
        // Check if this entry was filtered, and if not, add the retrieved
        // metadata.
        if (!util.isFakeEntry(requests[i].entry)) {
          item.isDlpRestricted = dlpMetadataList[j].isDlpRestricted;
          item.sourceUrl = dlpMetadataList[j].sourceUrl;
          item.isRestrictedForDestination =
              dlpMetadataList[j].isRestrictedForDestination;
          j++;
        }
        results.push(item);
      }
      return results;
    } catch (error) {
      console.warn(error);
      return requests.map(() => new MetadataItem());
    }
  }
}

/** @const {!Array<string>} */
DlpMetadataProvider.PROPERTY_NAMES = [
  'isDlpRestricted',
  'sourceUrl',
  'isRestrictedForDestination',
];
