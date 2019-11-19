// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Metadata containing thumbnail information.
 * @typedef {Object}
 */
let ThumbnailMetadataItem;

class ThumbnailModel {
  /**
   * @param {!MetadataModel} metadataModel
   */
  constructor(metadataModel) {
    /**
     * @private {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;
  }

  /**
   * @param {!Array<!Entry>} entries
   * @return {Promise<ThumbnailMetadataItem>} Promise fulfilled with old format
   *     metadata list.
   */
  get(entries) {
    const results = {};
    return this.metadataModel_
        .get(
            entries,
            [
              'modificationTime', 'customIconUrl', 'contentMimeType',
              'thumbnailUrl', 'croppedThumbnailUrl', 'present'
            ])
        .then(metadataList => {
          const contentRequestEntries = [];
          for (let i = 0; i < entries.length; i++) {
            const url = entries[i].toURL();
            // TODO(hirono): Use the provider results directly after removing
            // code using old metadata format.
            results[url] = {
              filesystem: {
                modificationTime: metadataList[i].modificationTime,
                modificationTimeError: metadataList[i].modificationTimeError
              },
              external: {
                thumbnailUrl: metadataList[i].thumbnailUrl,
                thumbnailUrlError: metadataList[i].thumbnailUrlError,
                croppedThumbnailUrl: metadataList[i].croppedThumbnailUrl,
                croppedThumbnailUrlError:
                    metadataList[i].croppedThumbnailUrlError,
                customIconUrl: metadataList[i].customIconUrl,
                customIconUrlError: metadataList[i].customIconUrlError,
                present: metadataList[i].present,
                presentError: metadataList[i].presentError
              },
              thumbnail: {},
              media: {}
            };
            const canUseContentThumbnail = metadataList[i].present &&
                (FileType.isImage(
                     entries[i], metadataList[i].contentMimeType) ||
                 FileType.isAudio(entries[i], metadataList[i].contentMimeType));
            if (canUseContentThumbnail) {
              contentRequestEntries.push(entries[i]);
            }
          }
          if (contentRequestEntries.length) {
            return this.metadataModel_
                .get(
                    contentRequestEntries,
                    [
                      'contentThumbnailUrl',
                      'contentThumbnailTransform',
                      'contentImageTransform',
                    ])
                .then(contentMetadataList => {
                  for (let i = 0; i < contentRequestEntries.length; i++) {
                    const url = contentRequestEntries[i].toURL();
                    results[url].thumbnail.url =
                        contentMetadataList[i].contentThumbnailUrl;
                    results[url].thumbnail.urlError =
                        contentMetadataList[i].contentThumbnailUrlError;
                    results[url].thumbnail.transform =
                        contentMetadataList[i].contentThumbnailTransform;
                    results[url].thumbnail.transformError =
                        contentMetadataList[i].contentThumbnailTransformError;
                    results[url].media.imageTransform =
                        contentMetadataList[i].contentImageTransform;
                    results[url].media.imageTransformError =
                        contentMetadataList[i].contentImageTransformError;
                  }
                });
          }
        })
        .then(() => {
          return entries.map(entry => {
            return results[entry.toURL()];
          });
        });
  }
}
