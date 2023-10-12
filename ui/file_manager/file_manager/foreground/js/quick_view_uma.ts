// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../../common/js/dialog_type.js';
import {FileType} from '../../common/js/file_type.js';
import {recordEnum} from '../../common/js/metrics.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

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
  private exportFileType_(entry: Entry, name: string) {
    let extension = FileType.getExtension(entry).toLowerCase();
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
  onEntryChanged(entry: FileEntry|Entry) {
    this.exportFileType_(entry, 'QuickView.FileType');
  }

  /**
   * Exports UMA based on the entry selected when Quick View is opened.
   */
  onOpened(entry: FileEntry|Entry, wayToOpen: WayToOpen) {
    this.exportFileType_(entry, 'QuickView.FileTypeOnLaunch');
    recordEnum('QuickView.WayToOpen', wayToOpen, WAY_TO_OPEN_ENUM_TO_INDEX);

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    const volumeType = volumeInfo && volumeInfo.volumeType;
    if (volumeType) {
      if (QUICK_VIEW_VOLUME_TYPES.includes(volumeType)) {
        recordEnum('QuickView.VolumeType', volumeType, QUICK_VIEW_VOLUME_TYPES);
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

/**
 * Keep the order of this in sync with FileManagerVolumeType in
 * tools/metrics/histograms/enums.xml.
 */
const QUICK_VIEW_VOLUME_TYPES = [
  VolumeManagerCommon.VolumeType.DRIVE,
  VolumeManagerCommon.VolumeType.DOWNLOADS,
  VolumeManagerCommon.VolumeType.REMOVABLE,
  VolumeManagerCommon.VolumeType.ARCHIVE,
  VolumeManagerCommon.VolumeType.PROVIDED,
  VolumeManagerCommon.VolumeType.MTP,
  VolumeManagerCommon.VolumeType.MEDIA_VIEW,
  VolumeManagerCommon.VolumeType.CROSTINI,
  VolumeManagerCommon.VolumeType.ANDROID_FILES,
  VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
  VolumeManagerCommon.VolumeType.SMB,
  VolumeManagerCommon.VolumeType.SYSTEM_INTERNAL,
  VolumeManagerCommon.VolumeType.GUEST_OS,
];
