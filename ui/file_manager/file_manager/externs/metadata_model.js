// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataItem} from '../foreground/js/metadata/metadata_item.js';

export class MetadataModelInterface {
  /**
   * Obtains metadata for entries.
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Promise<!Array<!MetadataItem>>}
   */
  get(entries, names) {}

  /**
   * Obtains metadata cache for entries.
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Array<!MetadataItem>}
   */
  getCache(entries, names) {}

  /**
   * Obtains metadata cache for file URLs.
   * @param {!Array<!string>} urls File URLs.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Array<!MetadataItem>}
   */
  getCacheByUrls(urls, names) {}

  /**
   * Invalidates metadata for updated entries.
   * @param {!Array<!Entry>} entries
   */
  notifyEntriesChanged(entries) {}

  /**
   * Updates the metadata of the given entries with the provided values for each
   * specified metadata name.
   * @param {!Array<!Entry>} entries FileURLs to have their metadata updated
   * @param {!Array<string>} names Metadata property names to be updated.
   * @param {!Array<!Array<string|number|boolean>>} values
   */
  update(entries, names, values) {}
}
