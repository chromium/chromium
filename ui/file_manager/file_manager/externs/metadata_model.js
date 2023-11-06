// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataItem} from '../foreground/js/metadata/metadata_item.js';

import {FilesAppEntry} from './files_app_entry_interfaces.js';

export class MetadataModelInterface {
  /**
   * Obtains metadata for entries.
   * @param {!Array<!Entry|!FilesAppEntry>} entries Entries.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Promise<!Array<!MetadataItem>>}
   */
  // @ts-ignore: error TS6133: 'names' is declared but its value is never read.
  get(entries, names) {
    return Promise.resolve([]);
  }

  /**
   * Obtains metadata cache for entries.
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Array<!MetadataItem>}
   */
  // @ts-ignore: error TS6133: 'names' is declared but its value is never read.
  getCache(entries, names) {
    return [];
  }

  /**
   * Obtains metadata cache for file URLs.
   * @param {!Array<!string>} urls File URLs.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Array<!MetadataItem>}
   */
  // @ts-ignore: error TS6133: 'names' is declared but its value is never read.
  getCacheByUrls(urls, names) {
    return [];
  }

  /**
   * Invalidates metadata for updated entries.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   */
  // @ts-ignore: error TS6133: 'entries' is declared but its value is never
  // read.
  notifyEntriesChanged(entries) {}

  /**
   * Updates the metadata of the given entries with the provided values for each
   * specified metadata name.
   * @param {!Array<!string>} entries FileURLs to have their metadata updated
   * @param {!Array<string>} names Metadata property names to be updated.
   * @param {!Array<!Array<string|number|boolean>>} values
   */
  // @ts-ignore: error TS6133: 'values' is declared but its value is never read.
  update(entries, names, values) {}
}
