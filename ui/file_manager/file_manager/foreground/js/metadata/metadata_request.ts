// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';

import type {MetadataKey} from './metadata_item.js';

export class MetadataRequest {
  /**
   * @param names Property name list to be requested.
   */
  constructor(public entry: Entry|FilesAppEntry, public names: MetadataKey[]) {}
}
