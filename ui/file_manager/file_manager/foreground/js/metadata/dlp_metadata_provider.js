// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    if (!requests.length) {
      return Promise.resolve([]);
    }

    // TODO(crbug.com/1297603): Early return if DLP isn't enabled.
    // TODO(crbug.com/1326932): Call chrome.fileManagerPrivate to check if the
    // file is managed.
    return Promise.all(requests.map(_request => new MetadataItem()));
  }
}

/** @const {!Array<string>} */
DlpMetadataProvider.PROPERTY_NAMES = [
  // TODO(crbug.com/1329770): Consider using an enum for this property.
  'isDlpRestricted',
];