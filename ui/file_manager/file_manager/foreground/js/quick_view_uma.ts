// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {getExtension} from '../../common/js/file_type.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {recordEnum} from '../../common/js/metrics.js';
import {UMA_VOLUME_TYPES} from '../../common/js/uma.js';
import {DialogType} from '../../state/state.js';

import {UMA_INDEX_KNOWN_EXTENSIONS} from './uma_enums.gen.js';

/**
 * UMA exporter for Quick View.
 */
export class QuickViewUma {
  constructor(
      private volumeManager_: VolumeManager, private dialogType_: DialogType) {}

  /**
   * Exports file type metric with the given histogram `name`.
   */
  private exportFileType_(entry: Entry|FilesAppEntry, name: string) {
    let extension = getExtension(entry).toLowerCase();
    if (entry.isDirectory) {
      extension = 'directory';
    } else if (extension === '') {
      extension = 'no extension';
    } else if (UMA_INDEX_KNOWN_EXTENSIONS.indexOf(extension) < 0) {
      extension = 'unknown extension';
    }
    recordEnum(name, extension, UMA_INDEX_KNOWN_EXTENSIONS);
  }

  /**
   * Exports UMA based on the entry shown in Quick View.
   */
  onEntryChanged(entry: Entry|FilesAppEntry) {
    this.exportFileType_(entry, 'QuickView.FileType');
  }

  /**
   * Exports UMA based on the entry selected when Quick View is opened.
   */
  onOpened(entry: Entry|FilesAppEntry, wayToOpen: WayToOpen) {
    this.exportFileType_(entry, 'QuickView.FileTypeOnLaunch');
    recordEnum('QuickView.WayToOpen', wayToOpen, WAY_TO_OPEN_ENUM_TO_INDEX);

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    const volumeType = volumeInfo && volumeInfo.volumeType;
    if (volumeType) {
      if (UMA_VOLUME_TYPES.includes(volumeType)) {
        recordEnum('QuickView.VolumeType', volumeType, UMA_VOLUME_TYPES);
      } else {
        console.warn('Unknown volume type: ' + volumeType);
      }
    } else {
      console.warn('Missing volume type');
    }
    // Record stats of dialog types. It must be in sync with
    // FileDialogType enum in tools/metrics/histograms/enums.xml.
    recordEnum('QuickView.DialogType', this.dialogType_, [
      DialogType.SELECT_FOLDER,
      DialogType.SELECT_UPLOAD_FOLDER,
      DialogType.SELECT_SAVEAS_FILE,
      DialogType.SELECT_OPEN_FILE,
      DialogType.SELECT_OPEN_MULTI_FILE,
      DialogType.FULL_PAGE,
    ]);
  }
}

/**
 * In which way quick view was opened.
 */
export const enum WayToOpen {
  CONTEXT_MENU = 'contextMenu',
  SPACE_KEY = 'spaceKey',
  SELECTION_MENU = 'selectionMenu',
}

/**
 * The order should be consistent with the definition in histograms.xml.
 */
const WAY_TO_OPEN_ENUM_TO_INDEX = [
  WayToOpen.CONTEXT_MENU,
  WayToOpen.SPACE_KEY,
  WayToOpen.SELECTION_MENU,
];
