// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * UMA exporter for Quick View.
 */
class QuickViewUma {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!DialogType} dialogType
   */
  constructor(volumeManager, dialogType) {
    /**
     * @type {!VolumeManager}
     * @private
     */
    this.volumeManager_ = volumeManager;
    /**
     * @type {DialogType}
     * @private
     */
    this.dialogType_ = dialogType;
  }

  /**
   * Exports file type metric with the given |name|.
   *
   * @param {!FileEntry} entry
   * @param {string} name The histogram name.
   *
   * @private
   */
  exportFileType_(entry, name) {
    let extension = FileType.getExtension(entry).toLowerCase();
    if (entry.isDirectory) {
      extension = 'directory';
    } else if (extension === '') {
      extension = 'no extension';
    } else if (FileTasks.UMA_INDEX_KNOWN_EXTENSIONS.indexOf(extension) < 0) {
      extension = 'unknown extension';
    }
    metrics.recordEnum(name, extension, FileTasks.UMA_INDEX_KNOWN_EXTENSIONS);
  }

  /**
   * Exports UMA based on the entry shown in Quick View.
   *
   * @param {!FileEntry} entry
   */
  onEntryChanged(entry) {
    this.exportFileType_(entry, 'QuickView.FileType');
  }

  /**
   * Exports UMA based on the entry selected when Quick View is opened.
   *
   * @param {!FileEntry} entry
   * @param {QuickViewUma.WayToOpen} wayToOpen
   */
  onOpened(entry, wayToOpen) {
    this.exportFileType_(entry, 'QuickView.FileTypeOnLaunch');
    metrics.recordEnum(
        'QuickView.WayToOpen', wayToOpen, QuickViewUma.WayToOpenValues_);

    const volumeType = this.volumeManager_.getVolumeInfo(entry).volumeType;
    if (QuickViewUma.VolumeType.includes(volumeType)) {
      metrics.recordEnum(
          'QuickView.VolumeType', volumeType, QuickViewUma.VolumeType);
    } else {
      console.error('Unknown volume type: ' + volumeType);
    }
    // Record stats of dialog types. It must be in sync with
    // FileDialogType enum in tools/metrics/histograms/histogram.xml.
    metrics.recordEnum('QuickView.DialogType', this.dialogType_, [
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
 * @enum {string}
 * @const
 */
QuickViewUma.WayToOpen = {
  CONTEXT_MENU: 'contextMenu',
  SPACE_KEY: 'spaceKey',
  SELECTION_MENU: 'selectionMenu',
};

/**
 * The order should be consistnet with the definition  in histograms.xml.
 *
 * @const {!Array<QuickViewUma.WayToOpen>}
 * @private
 */
QuickViewUma.WayToOpenValues_ = [
  QuickViewUma.WayToOpen.CONTEXT_MENU,
  QuickViewUma.WayToOpen.SPACE_KEY,
  QuickViewUma.WayToOpen.SELECTION_MENU,
];

/**
 * Keep the order of this in sync with FileManagerVolumeType in
 * tools/metrics/histograms/histograms.xml.
 *
 * @type {!Array<VolumeManagerCommon.VolumeType>}
 * @const
 */
QuickViewUma.VolumeType = [
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
];
