// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {FileType} from '../../common/js/file_type.js';
import {TrashEntry} from '../../common/js/trash.js';
import {util} from '../../common/js/util.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {FilesQuickView} from '../elements/files_quick_view.js';

import {MetadataItem} from './metadata/metadata_item.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {PathComponent} from './path_component.js';
import {QuickViewModel} from './quick_view_model.js';
import {FileMetadataFormatter} from './ui/file_metadata_formatter.js';

/**
 * Controller of metadata box.
 * This should be initialized with |init| method.
 */
export class MetadataBoxController {
  /**
   * @param {!MetadataModel} metadataModel
   * @param {!QuickViewModel} quickViewModel
   * @param {!FileMetadataFormatter} fileMetadataFormatter
   * @param {!VolumeManager} volumeManager
   */
  constructor(
      metadataModel, quickViewModel, fileMetadataFormatter, volumeManager) {
    /**
     * @type {!MetadataModel}
     * @private
     */
    this.metadataModel_ = metadataModel;

    /**
     * @type {!QuickViewModel}
     * @private
     */
    this.quickViewModel_ = quickViewModel;

    /**
     * @type {FilesMetadataBoxElement} metadataBox
     * @private
     */
    this.metadataBox_ = null;

    /**
     * @type {FilesQuickView} quickView
     * @private
     */
    this.quickView_ = null;

    /**
     * @type {!FileMetadataFormatter}
     * @private
     */
    this.fileMetadataFormatter_ = fileMetadataFormatter;

    /**
     * @type {!VolumeManager}
     * @private
     */
    this.volumeManager_ = volumeManager;

    /**
     * @type {Entry}
     * @private
     */
    this.previousEntry_ = null;

    /**
     * @type {boolean}
     * @private
     */
    this.isDirectorySizeLoading_ = false;

    /**
     * @type {?function(!DirectoryEntry)}
     * @private
     */
    this.onDirectorySizeLoaded_ = null;
  }

  /**
   * Initialize the controller with quick view which will be lazily loaded.
   *
   * @param{!FilesQuickView} quickView
   */
  init(quickView) {
    this.quickView_ = quickView;

    this.fileMetadataFormatter_.addEventListener(
        'date-time-format-changed', this.updateView_.bind(this));

    this.quickView_.addEventListener(
        'metadata-box-active-changed', this.updateView_.bind(this));

    this.quickViewModel_.addEventListener(
        'selected-entry-changed', this.updateView_.bind(this));

    this.metadataBox_ = this.quickView_.getFilesMetadataBox();
    this.metadataBox_.clear(false);

    this.metadataBox_.setAttribute('files-ng', '');
  }

  /**
   * Update the view of metadata box.
   * @param {!Event} event
   *
   * @private
   */
  updateView_(event) {
    if (!this.quickView_.metadataBoxActive) {
      return;
    }

    const entry = this.quickViewModel_.getSelectedEntry();
    const isSameEntry = util.isSameEntry(entry, this.previousEntry_);
    this.previousEntry_ = entry;

    if (!entry) {
      this.metadataBox_.clear(false);
      return;
    }

    if (event.type === 'date-time-format-changed') {
      // Update the displayed entry modificationTime format only, and return.
      this.metadataModel_.get([entry], ['modificationTime'])
          .then(this.updateModificationTime_.bind(this, entry));
      return;
    }

    // Do not clear isSizeLoading and size fields when the entry is not changed.
    this.metadataBox_.clear(isSameEntry);

    const metadata = MetadataBoxController.GENERAL_METADATA_NAMES.concat(
        ['alternateUrl', 'externalFileUrl', 'hosted']);
    this.metadataModel_.get([entry], metadata)
        .then(this.onGeneralMetadataLoaded_.bind(this, entry, isSameEntry));
  }

  /**
   * Updates the metadata box with general and file-specific metadata.
   *
   * @param {!Entry} entry
   * @param {boolean} isSameEntry if the entry is not changed from the last
   *     time.
   * @param {!Array<!MetadataItem>} items
   *
   * @private
   */
  onGeneralMetadataLoaded_(entry, isSameEntry, items) {
    const type = FileType.getType(entry).type;
    const item = items[0];

    if (entry.isDirectory) {
      const directory = /** @type {!DirectoryEntry} */ (entry);
      this.setDirectorySize_(directory, isSameEntry);
    } else if (item.size) {
      this.metadataBox_.size =
          this.fileMetadataFormatter_.formatSize(item.size, item.hosted, true);
      this.metadataBox_.metadataRendered('size');
    }

    if (entry.restoreEntry) {
      this.metadataBox_.originalLocation =
          this.getFileLocationLabel_(entry.restoreEntry);
      this.metadataBox_.metadataRendered('originalLocation');
    }

    this.updateModificationTime_(entry, items);

    if (!entry.isDirectory) {
      let media = [];  // Extra metadata types for local video media.

      let sniffMimeType = 'mediaMimeType';
      if (item.externalFileUrl || item.alternateUrl) {
        sniffMimeType = 'contentMimeType';
      } else if (type === 'video') {
        media = MetadataBoxController.EXTRA_METADATA_NAMES;
      }

      this.metadataModel_.get([entry], [sniffMimeType].concat(media))
          .then(items => {
            this.metadataBox_.mediaMimeType = items[0][sniffMimeType] || '';
            this.metadataBox_.metadataRendered('mime');
          })
          .then(() => {
            this.metadataBox_.fileLocation = this.getFileLocationLabel_(entry);
            this.metadataBox_.metadataRendered('location');
          });
    }

    if (['image', 'video', 'audio'].includes(type)) {
      if (item.externalFileUrl || item.alternateUrl) {
        const data = ['imageHeight', 'imageWidth'];
        this.metadataModel_.get([entry], data).then(items => {
          this.metadataBox_.imageWidth = items[0].imageWidth || 0;
          this.metadataBox_.imageHeight = items[0].imageHeight || 0;
          this.metadataBox_.setFileTypeInfo(type);
          this.metadataBox_.metadataRendered('meta');
        });
      } else {
        const data = MetadataBoxController.EXTRA_METADATA_NAMES;
        this.metadataModel_.get([entry], data).then(items => {
          const item = items[0];
          this.metadataBox_.setProperties({
            ifd: item.ifd || null,
            imageHeight: item.imageHeight || 0,
            imageWidth: item.imageWidth || 0,
            mediaAlbum: item.mediaAlbum || '',
            mediaArtist: item.mediaArtist || '',
            mediaDuration: item.mediaDuration || 0,
            mediaGenre: item.mediaGenre || '',
            mediaTitle: item.mediaTitle || '',
            mediaTrack: item.mediaTrack || '',
            mediaYearRecorded: item.mediaYearRecorded || '',
          });
          this.metadataBox_.setFileTypeInfo(type);
          this.metadataBox_.metadataRendered('meta');
        });
      }
    } else if (type === 'raw') {
      const data = ['ifd'];
      this.metadataModel_.get([entry], data).then(items => {
        const raw = items[0].ifd ? items[0].ifd : {};
        this.metadataBox_.ifd = items[0].ifd ? {raw} : null;
        this.metadataBox_.imageWidth = raw.width || 0;
        this.metadataBox_.imageHeight = raw.height || 0;
        this.metadataBox_.setFileTypeInfo('image');
        this.metadataBox_.metadataRendered('meta');
      });
    }
  }

  /**
   * Updates the metadata box modificationTime.
   *
   * @param {!Entry} entry
   * @param {!Array<!MetadataItem>} items
   *
   * @private
   */
  updateModificationTime_(entry, items) {
    const item = items[0];

    this.metadataBox_.modificationTime =
        this.fileMetadataFormatter_.formatModDate(item.modificationTime);
  }

  /**
   * Set a current directory's size in metadata box.
   *
   * A loading animation is shown while fetching the directory size. However, it
   * won't show if there is no size value. Use a dummy value ' ' in that case.
   *
   * To avoid flooding the OS system with chrome.getDirectorySize requests, if a
   * previous request is active, store the new request and return. Only the most
   * recent new request is stored. When the active request returns, it calls the
   * stored request instead of updating the size field.
   *
   * @param {!DirectoryEntry} entry
   * @param {boolean} isSameEntry True if the entry is not changed from the last
   *    time. False enables the loading animation.
   *
   * @private
   */
  setDirectorySize_(entry, isSameEntry) {
    assert(entry.isDirectory);

    if (this.metadataBox_.size === '') {
      this.metadataBox_.size = ' ';  // Provide a dummy size value.
    }

    if (this.isDirectorySizeLoading_) {
      if (!isSameEntry) {
        this.metadataBox_.isSizeLoading = true;
      }

      // Store the new setDirectorySize_ request and return.
      this.onDirectorySizeLoaded_ = lastEntry => {
        this.setDirectorySize_(entry, util.isSameEntry(entry, lastEntry));
      };
      return;
    }

    this.metadataBox_.isSizeLoading = !isSameEntry;

    this.isDirectorySizeLoading_ = true;
    chrome.fileManagerPrivate.getDirectorySize(entry, size => {
      this.isDirectorySizeLoading_ = false;

      if (this.onDirectorySizeLoaded_) {
        setTimeout(this.onDirectorySizeLoaded_.bind(null, entry));
        this.onDirectorySizeLoaded_ = null;
        return;
      }

      if (this.quickViewModel_.getSelectedEntry() != entry) {
        return;
      }

      if (chrome.runtime.lastError) {
        console.warn(chrome.runtime.lastError);
        size = undefined;
      }

      this.metadataBox_.size =
          this.fileMetadataFormatter_.formatSize(size, true, true);
      this.metadataBox_.isSizeLoading = false;
      this.metadataBox_.metadataRendered('size');
    });
  }

  /**
   * Returns a label to display the file's location.
   * @param {!Entry} entry
   * @return {string}
   * @private
   */
  getFileLocationLabel_(entry) {
    const components =
        PathComponent.computeComponentsFromEntry(entry, this.volumeManager_);
    return components.map(c => c.name).join('/');
  }
}

/**
 * @const {!Array<string>}
 */
MetadataBoxController.GENERAL_METADATA_NAMES = [
  'size',
  'modificationTime',
];

/**
 * @const {!Array<string>}
 */
MetadataBoxController.EXTRA_METADATA_NAMES = [
  'ifd',
  'imageHeight',
  'imageWidth',
  'mediaAlbum',
  'mediaArtist',
  'mediaDuration',
  'mediaGenre',
  'mediaTitle',
  'mediaTrack',
  'mediaYearRecorded',
];
