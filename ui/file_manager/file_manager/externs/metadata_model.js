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
   * Invalidates metadata for updated entries.
   * @param {!Array<!Entry>} entries
   */
  notifyEntriesChanged(entries) {}
}
