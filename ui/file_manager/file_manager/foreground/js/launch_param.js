// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../../common/js/dialog_type.js';
import {FilesAppState} from '../../common/js/files_app_state.js';
import {AllowedPaths} from '../../common/js/volume_manager_types.js';

/**
 * Parsed options used to launch a new Files app window.
 */
export class LaunchParam {
  /**
   * @param {!FilesAppState} unformatted Unformatted option.
   */
  constructor(unformatted) {
    /**
     * @type {DialogType}
     * @const
     */
    this.type = unformatted.type || DialogType.FULL_PAGE;

    /**
     * @type {string}
     * @const
     */
    this.currentDirectoryURL =
        unformatted.currentDirectoryURL ? unformatted.currentDirectoryURL : '';

    /**
     * @type {string}
     * @const
     */
    this.selectionURL =
        unformatted.selectionURL ? unformatted.selectionURL : '';

    /**
     * @type {string}
     * @const
     */
    this.targetName = unformatted.targetName ? unformatted.targetName : '';

    /**
     * @type {!Array<!Object>}
     * @const
     */
    this.typeList = unformatted.typeList ? unformatted.typeList : [];

    /**
     * @type {boolean}
     * @const
     */
    this.includeAllFiles = !!unformatted.includeAllFiles;

    /**
     * @type {!AllowedPaths}
     * @const
     */
    this.allowedPaths = unformatted.allowedPaths ? unformatted.allowedPaths :
                                                   AllowedPaths.ANY_PATH_OR_URL;

    /**
     * @type {string}
     * @const
     */
    this.searchQuery = unformatted.searchQuery || '';

    /**
     * @type {boolean}
     * @const
     */
    this.showAndroidPickerApps = !!unformatted.showAndroidPickerApps;

    /**
     * @type {!Array<string>}
     * @const
     */
    this.volumeFilter = unformatted.volumeFilter || [];
  }
}
