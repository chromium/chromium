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
  // The following fields are described in detail in the FilesAppState class.

  readonly type: DialogType;
  readonly currentDirectoryURL: string;
  readonly selectionURL: string;
  readonly targetName: string;
  readonly typeList: Object[];
  readonly includeAllFiles: boolean;
  readonly allowedPaths: string;
  readonly searchQuery: string;
  readonly showAndroidPickerApps: boolean;
  readonly volumeFilter: string[];

  constructor(unformatted: FilesAppState) {
    this.type = unformatted.type || DialogType.FULL_PAGE;
    this.currentDirectoryURL = unformatted.currentDirectoryURL ?? '';
    this.selectionURL = unformatted.selectionURL ?? '';
    this.targetName = unformatted.targetName ?? '';
    this.typeList = unformatted.typeList ?? [];
    this.includeAllFiles = !!unformatted.includeAllFiles;
    this.allowedPaths =
        unformatted.allowedPaths ?? AllowedPaths.ANY_PATH_OR_URL;
    this.searchQuery = unformatted.searchQuery ?? '';
    this.showAndroidPickerApps = !!unformatted.showAndroidPickerApps;
    this.volumeFilter = unformatted.volumeFilter ?? [];
  }
}
