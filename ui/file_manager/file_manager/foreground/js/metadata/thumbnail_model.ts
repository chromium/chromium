// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isAudio, isImage} from '../../../common/js/file_type.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';

import type {ImageTransformation} from './metadata_item.js';
import type {MetadataModel} from './metadata_model.js';

/**
 * Metadata containing thumbnail information.
 */
export interface ThumbnailMetadataItem {
  filesystem: {modificationTime?: Date, modificationTimeError?: Error};
  external: {
    thumbnailUrl?: string,
    thumbnailUrlError?: Error,
    croppedThumbnailUrl?: string,
    croppedThumbnailUrlError?: Error,
    customIconUrl?: string,
    customIconUrlError?: Error,
    present?: boolean,
    presentError?: Error,
  };
  thumbnail: {
    url?: string,
    urlError?: Error&{errorDescription?: string},
    transform?: ImageTransformation,
    transformError?: Error,
  };
  media: {imageTransform?: ImageTransformation, imageTransformError?: Error};
  contentMimeType?: string;
}

export class ThumbnailModel {
  constructor(private readonly metadataModel_: MetadataModel) {}

  /**
   * @return Promise fulfilled with old format metadata list.
   */
  async get(entries: Array<Entry|FilesAppEntry>):
      Promise<ThumbnailMetadataItem[]> {
    const results: Record<string, ThumbnailMetadataItem> = {};
    const metadataList = await this.metadataModel_.get(entries, [
      'modificationTime',
      'customIconUrl',
      'contentMimeType',
      'thumbnailUrl',
      'croppedThumbnailUrl',
      'present',
    ]);

    const contentRequestEntries: Array<Entry|FilesAppEntry> = [];
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i]!;
      const metadata = metadataList[i]!;
      const url = entry.toURL();

      // TODO(hirono): Use the provider results directly after removing
      // code using old metadata format.
      results[url] = {
        filesystem: {
          modificationTime: metadata.modificationTime,
          modificationTimeError: metadata.modificationTimeError,
        },
        external: {
          thumbnailUrl: metadata.thumbnailUrl,
          thumbnailUrlError: metadata.thumbnailUrlError,
          croppedThumbnailUrl: metadata.croppedThumbnailUrl,
          croppedThumbnailUrlError: metadata.croppedThumbnailUrlError,
          customIconUrl: metadata.customIconUrl,
          customIconUrlError: metadata.customIconUrlError,
          present: metadata.present,
          presentError: metadata.presentError,
        },
        thumbnail: {},
        media: {},
      };
      const canUseContentThumbnail = metadata.present &&
          (isImage(entry, metadata.contentMimeType) ||
           isAudio(entry, metadata.contentMimeType));
      if (canUseContentThumbnail) {
        contentRequestEntries.push(entry);
      }
    }
    if (contentRequestEntries.length) {
      const contentMetadataList =
          await this.metadataModel_.get(contentRequestEntries, [
            'contentThumbnailUrl',
            'contentThumbnailTransform',
            'contentImageTransform',
          ]);
      for (let i = 0; i < contentRequestEntries.length; i++) {
        const url = contentRequestEntries[i]!.toURL();
        const contentMetadata = contentMetadataList[i]!;
        const result = results[url]!;

        result.thumbnail.url = contentMetadata.contentThumbnailUrl;
        result.thumbnail.urlError = contentMetadata.contentThumbnailUrlError;
        result.thumbnail.transform = contentMetadata.contentThumbnailTransform;
        result.thumbnail.transformError =
            contentMetadata.contentThumbnailTransformError;
        result.media.imageTransform = contentMetadata.contentImageTransform;
        result.media.imageTransformError =
            contentMetadata.contentImageTransformError;
      }
    }

    return entries.map(entry => {
      return results[entry.toURL()]!;
    });
  }
}
