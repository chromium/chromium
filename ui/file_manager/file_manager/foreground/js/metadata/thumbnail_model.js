// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileType} from '../../../common/js/file_type.js';

import {MetadataModel} from './metadata_model.js';

/**
 * Metadata containing thumbnail information.
 * @typedef {Object}
 */
let ThumbnailMetadataItem;

export class ThumbnailModel {
  /**
   * @param {!MetadataModel} metadataModel
   */
  constructor(metadataModel) {
    /**
     * @private @type {!MetadataModel}
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
              'modificationTime',
              'customIconUrl',
              'contentMimeType',
              'thumbnailUrl',
              'croppedThumbnailUrl',
              'present',
            ])
        // @ts-ignore: error TS7030: Not all code paths return a value.
        .then(metadataList => {
          // @ts-ignore: error TS7034: Variable 'contentRequestEntries'
          // implicitly has type 'any[]' in some locations where its type cannot
          // be determined.
          const contentRequestEntries = [];
          for (let i = 0; i < entries.length; i++) {
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            const url = entries[i].toURL();
            // TODO(hirono): Use the provider results directly after removing
            // code using old metadata format.
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // '{}'.
            results[url] = {
              filesystem: {
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                modificationTime: metadataList[i].modificationTime,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                modificationTimeError: metadataList[i].modificationTimeError,
              },
              external: {
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                thumbnailUrl: metadataList[i].thumbnailUrl,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                thumbnailUrlError: metadataList[i].thumbnailUrlError,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                croppedThumbnailUrl: metadataList[i].croppedThumbnailUrl,
                croppedThumbnailUrlError:
                    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                    metadataList[i].croppedThumbnailUrlError,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                customIconUrl: metadataList[i].customIconUrl,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                customIconUrlError: metadataList[i].customIconUrlError,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                present: metadataList[i].present,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                presentError: metadataList[i].presentError,
              },
              thumbnail: {},
              media: {},
            };
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            const canUseContentThumbnail = metadataList[i].present &&
                (FileType.isImage(
                     // @ts-ignore: error TS2532: Object is possibly
                     // 'undefined'.
                     entries[i], metadataList[i].contentMimeType) ||
                 // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                 FileType.isAudio(entries[i], metadataList[i].contentMimeType));
            if (canUseContentThumbnail) {
              contentRequestEntries.push(entries[i]);
            }
          }
          if (contentRequestEntries.length) {
            return this.metadataModel_
                .get(
                    // @ts-ignore: error TS2345: Argument of type
                    // '(FileSystemEntry | undefined)[]' is not assignable to
                    // parameter of type 'FileSystemEntry[]'.
                    contentRequestEntries,
                    [
                      'contentThumbnailUrl',
                      'contentThumbnailTransform',
                      'contentImageTransform',
                    ])
                .then(contentMetadataList => {
                  for (let i = 0; i < contentRequestEntries.length; i++) {
                    // @ts-ignore: error TS7005: Variable
                    // 'contentRequestEntries' implicitly has an 'any[]' type.
                    const url = contentRequestEntries[i].toURL();
                    // @ts-ignore: error TS7053: Element implicitly has an 'any'
                    // type because expression of type 'any' can't be used to
                    // index type '{}'.
                    results[url].thumbnail.url =
                        // @ts-ignore: error TS2532: Object is possibly
                        // 'undefined'.
                        contentMetadataList[i].contentThumbnailUrl;
                    // @ts-ignore: error TS7053: Element implicitly has an 'any'
                    // type because expression of type 'any' can't be used to
                    // index type '{}'.
                    results[url].thumbnail.urlError =
                        // @ts-ignore: error TS2532: Object is possibly
                        // 'undefined'.
                        contentMetadataList[i].contentThumbnailUrlError;
                    // @ts-ignore: error TS7053: Element implicitly has an 'any'
                    // type because expression of type 'any' can't be used to
                    // index type '{}'.
                    results[url].thumbnail.transform =
                        // @ts-ignore: error TS2532: Object is possibly
                        // 'undefined'.
                        contentMetadataList[i].contentThumbnailTransform;
                    // @ts-ignore: error TS7053: Element implicitly has an 'any'
                    // type because expression of type 'any' can't be used to
                    // index type '{}'.
                    results[url].thumbnail.transformError =
                        // @ts-ignore: error TS2532: Object is possibly
                        // 'undefined'.
                        contentMetadataList[i].contentThumbnailTransformError;
                    // @ts-ignore: error TS7053: Element implicitly has an 'any'
                    // type because expression of type 'any' can't be used to
                    // index type '{}'.
                    results[url].media.imageTransform =
                        // @ts-ignore: error TS2532: Object is possibly
                        // 'undefined'.
                        contentMetadataList[i].contentImageTransform;
                    // @ts-ignore: error TS7053: Element implicitly has an 'any'
                    // type because expression of type 'any' can't be used to
                    // index type '{}'.
                    results[url].media.imageTransformError =
                        // @ts-ignore: error TS2532: Object is possibly
                        // 'undefined'.
                        contentMetadataList[i].contentImageTransformError;
                  }
                });
          }
        })
        .then(() => {
          return entries.map(entry => {
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // '{}'.
            return results[entry.toURL()];
          });
        });
  }
}
