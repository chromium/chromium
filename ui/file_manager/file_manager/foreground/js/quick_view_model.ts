// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';

/**
 * Quick view model that doesn't fit into properties of quick view element.
 */
export class QuickViewModel extends EventTarget {
  private selectedEntry_?: Entry|FilesAppEntry;

  constructor() {
    super();
  }

  /**
   * Returns the selected file entry.
   */
  getSelectedEntry() {
    return this.selectedEntry_;
  }

  /**
   * Sets the selected file entry. Emits a synchronous selected-entry-changed
   * event to immediately call MetadataBoxController.updateView_().
   */
  setSelectedEntry(entry: Entry|FilesAppEntry) {
    this.selectedEntry_ = entry;
    dispatchSimpleEvent(this, 'selected-entry-changed');
  }
}
