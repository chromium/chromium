// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isDirectoryEntry, isNativeEntry, isSameEntry, unwrapEntry} from '../../common/js/entry_utils.js';
import {getType} from '../../common/js/file_type.js';
import type {FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {strf} from '../../common/js/translations.js';
import type {TrashEntry} from '../../common/js/trash.js';
import type {FilesMetadataBox} from '../elements/files_metadata_box.js';
import {type RawIfd} from '../elements/files_metadata_box.js';
import type {FilesQuickView} from '../elements/files_quick_view.js';

import type {MetadataItem} from './metadata/metadata_item.js';
import {type MetadataKey} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import {PathComponent} from './path_component.js';
import type {QuickViewModel} from './quick_view_model.js';
import type {FileMetadataFormatter} from './ui/file_metadata_formatter.js';

function isTrashEntry(entry: Entry|FilesAppEntry): entry is TrashEntry {
  return 'restoreEntry' in entry;
}

/**
 * Controller of metadata box.
 * This should be initialized with |init| method.
 */
export class MetadataBoxController {
  private metadataBox_: FilesMetadataBox|null = null;

  private quickView_: FilesQuickView|null = null;

  private previousEntry_?: Entry|FilesAppEntry;

  private isDirectorySizeLoading_ = false;

  private onDirectorySizeLoaded_:
      ((entry: DirectoryEntry|FilesAppDirEntry) => void)|null = null;

  constructor(
      private metadataModel_: MetadataModel,
      private quickViewModel_: QuickViewModel,
      private fileMetadataFormatter_: FileMetadataFormatter,
      private volumeManager_: VolumeManager) {}

  /**
   * Initialize the controller with quick view which will be lazily loaded.
   */
  init(quickView: FilesQuickView) {
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
   */
  private updateView_(event: Event) {
    if (!this.quickView_?.metadataBoxActive) {
      return;
    }

    const entry = this.quickViewModel_.getSelectedEntry()!;
    const sameEntry = isSameEntry(entry, this.previousEntry_);
    this.previousEntry_ = entry;

    if (!entry) {
      this.metadataBox?.clear(false);
      return;
    }

    if (event.type === 'date-time-format-changed') {
      // Update the displayed entry modificationTime format only, and return.
      this.metadataModel_.get([entry], ['modificationTime'])
          .then(this.updateModificationTime_.bind(this, entry));
      return;
    }

    // Do not clear isSizeLoading and size fields when the entry is not changed.
    this.metadataBox.clear(sameEntry);

    const metadata = [
      ...GENERAL_METADATA_NAMES,
      'alternateUrl',
      'externalFileUrl',
      'hosted',
    ] as const;
    this.metadataModel_.get([entry], metadata)
        .then(this.onGeneralMetadataLoaded_.bind(this, entry, sameEntry));
  }

  /**
   * Accessor to get a guaranteed `FilesMetadataBox`.
   */
  private get metadataBox(): FilesMetadataBox {
    return this.metadataBox_!;
  }

  /**
   * Updates the metadata box with general and file-specific metadata.
   *
   * @param isSameEntry if the entry is not changed from the last time.
   */
  private onGeneralMetadataLoaded_(
      entry: Entry|FilesAppEntry, isSameEntry: boolean, items: MetadataItem[]) {
    const type = getType(entry).type;
    const item = items[0];

    if (isDirectoryEntry(entry)) {
      this.setDirectorySize_(entry, isSameEntry);
    } else if (item?.size) {
      this.metadataBox.size =
          this.fileMetadataFormatter_.formatSize(item.size, item.hosted, true);
      this.metadataBox.metadataRendered('size');
    }

    if (isTrashEntry(entry)) {
      this.metadataBox.originalLocation =
          this.getFileLocationLabel_(entry.restoreEntry);
      this.metadataBox.metadataRendered('originalLocation');
    }

    this.updateModificationTime_(entry, items);

    if (!entry.isDirectory) {
      // Extra metadata types for local video media.
      let media: readonly MetadataKey[] = [];

      let sniffMimeType: MetadataKey = 'mediaMimeType';
      if (item?.externalFileUrl || item?.alternateUrl) {
        sniffMimeType = 'contentMimeType';
      } else if (type === 'video') {
        media = EXTRA_METADATA_NAMES;
      }

      this.metadataModel_.get([entry], [sniffMimeType, ...media])
          .then(items => {
            let mimeType = items[0] &&
                    items[0][sniffMimeType as keyof MetadataItem] as string ||
                '';
            const newType = getType(entry, mimeType);
            if (newType.encrypted) {
              mimeType =
                  strf('METADATA_BOX_ENCRYPTED', newType.originalMimeType);
            }
            this.metadataBox.mediaMimeType = mimeType;
            this.metadataBox.metadataRendered('mime');
            this.metadataBox.fileLocation = this.getFileLocationLabel_(entry);
            this.metadataBox.metadataRendered('location');
          });
    }

    if (['image', 'video', 'audio'].includes(type)) {
      if (item?.externalFileUrl || item?.alternateUrl) {
        this.metadataModel_.get([entry], ['imageHeight', 'imageWidth'])
            .then(items => {
              this.metadataBox.imageWidth = items[0]?.imageWidth || 0;
              this.metadataBox.imageHeight = items[0]?.imageHeight || 0;
              this.metadataBox.setFileTypeInfo(type);
              this.metadataBox.metadataRendered('meta');
            });
      } else {
        const data = EXTRA_METADATA_NAMES;
        this.metadataModel_.get([entry], data).then(items => {
          const item = items[0]!;
          this.metadataBox.setProperties({
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
          this.metadataBox.setFileTypeInfo(type);
          this.metadataBox.metadataRendered('meta');
        });
      }
    } else if (type === 'raw') {
      this.metadataModel_.get([entry], ['ifd']).then(items => {
        const raw: RawIfd|null = items[0]?.ifd ? items[0].ifd as RawIfd : null;
        this.metadataBox.ifd = raw ? {raw} : undefined;
        this.metadataBox.imageWidth = raw?.width || 0;
        this.metadataBox.imageHeight = raw?.height || 0;
        this.metadataBox.setFileTypeInfo('image');
        this.metadataBox.metadataRendered('meta');
      });
    }
  }

  /**
   * Updates the metadata box modificationTime.
   */
  private updateModificationTime_(
      _: Entry|FilesAppEntry, items: MetadataItem[]) {
    const item = items[0];

    this.metadataBox.modificationTime =
        this.fileMetadataFormatter_.formatModDate(item?.modificationTime);
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
   * `isSameEntry` is True if the entry is not changed from the last time. False
   * enables the loading animation.
   */
  private setDirectorySize_(
      entry: DirectoryEntry|FilesAppDirEntry, sameEntry: boolean) {
    if (!isDirectoryEntry(entry)) {
      return;
    }
    const directoryEntry = unwrapEntry(entry);
    if (!isNativeEntry(directoryEntry)) {
      const typeName = ('typeName' in directoryEntry) ?
          directoryEntry.typeName :
          'no typeName';
      console.warn('Supplied directory is not a native type:', typeName);
      return;
    }

    if (this.metadataBox.size === '') {
      this.metadataBox.size = ' ';  // Provide a dummy size value.
    }

    if (this.isDirectorySizeLoading_) {
      if (!sameEntry) {
        this.metadataBox.isSizeLoading = true;
      }

      // Store the new setDirectorySize_ request and return.
      this.onDirectorySizeLoaded_ = lastEntry => {
        this.setDirectorySize_(entry, isSameEntry(entry, lastEntry));
      };
      return;
    }

    this.metadataBox.isSizeLoading = !sameEntry;

    this.isDirectorySizeLoading_ = true;
    chrome.fileManagerPrivate.getDirectorySize(
        directoryEntry as DirectoryEntry, (size: number|undefined) => {
          this.isDirectorySizeLoading_ = false;

          if (this.onDirectorySizeLoaded_) {
            setTimeout(this.onDirectorySizeLoaded_.bind(null, entry));
            this.onDirectorySizeLoaded_ = null;
            return;
          }

          if (this.quickViewModel_.getSelectedEntry() !== entry) {
            return;
          }

          if (chrome.runtime.lastError) {
            console.warn(chrome.runtime.lastError);
            size = undefined;
          }

          this.metadataBox.size =
              this.fileMetadataFormatter_.formatSize(size, true, true);
          this.metadataBox.isSizeLoading = false;
          this.metadataBox.metadataRendered('size');
        });
  }

  /**
   * Returns a label to display the file's location.
   */
  private getFileLocationLabel_(entry: Entry|FilesAppEntry) {
    const components =
        PathComponent.computeComponentsFromEntry(entry, this.volumeManager_);
    return components.map(c => c.name).join('/');
  }
}

export const GENERAL_METADATA_NAMES = [
  'size',
  'modificationTime',
] as const;

export const EXTRA_METADATA_NAMES = [
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
] as const;
