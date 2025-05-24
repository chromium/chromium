// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import {createForUrl, type LoadImageResponse, LoadImageResponseStatus} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';

import {getContentMetadata, getContentMimeType} from '../../../common/js/api.js';
import {unwrapEntry} from '../../../common/js/entry_utils.js';
import {getType} from '../../../common/js/file_type.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {getSanitizedScriptUrl} from '../../../common/js/trusted_script_url_policy_util.js';
import {testSendMessage} from '../../../common/js/util.js';
import {THUMBNAIL_MAX_HEIGHT, THUMBNAIL_MAX_WIDTH} from '../thumbnail_loader.js';

import type {ParserMetadata} from './metadata_item.js';
import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import type {MetadataRequest} from './metadata_request.js';

const WORKER_SCRIPT = 'foreground/js/metadata/metadata_dispatcher.js';

/** @final */
export class ContentMetadataProvider extends MetadataProvider {
  static readonly PROPERTY_NAMES = [
    'contentImageTransform',
    'contentThumbnailTransform',
    'contentThumbnailUrl',
    'exifLittleEndian',
    'ifd',
    'imageHeight',
    'imageWidth',
    'mediaAlbum',
    'mediaArtist',
    'mediaDuration',
    'mediaGenre',
    'mediaMimeType',
    'mediaTitle',
    'mediaTrack',
    'mediaYearRecorded',
  ];

  /**
   * Map from Entry.toURL() to callback.
   * Note that simultaneous requests for same url are handled in MetadataCache.
   */
  private callbacks_: Record<string, Array<(item: MetadataItem) => void>> = {};

  private readonly dispatcher_: MessagePort;

  /**
   * @param messagePort Message port overriding the default worker port.
   */
  constructor(messagePort?: MessagePort) {
    super(ContentMetadataProvider.PROPERTY_NAMES);

    // Set up |this.disapatcher_|. Creates the Shared Worker if needed.
    this.dispatcher_ = this.createSharedWorker_(messagePort);
    this.dispatcher_.onmessage = this.onMessage_.bind(this);
    this.dispatcher_.onmessageerror = (error) => {
      console.warn('ContentMetadataProvider worker msg error:', error);
    };
    this.dispatcher_.postMessage({verb: 'init'});
    this.dispatcher_.start();
  }

  /**
   * Returns |messagePort| if given. Otherwise creates the Shared Worker
   * and returns its message port.
   */
  private createSharedWorker_(messagePort?: MessagePort): MessagePort {
    if (messagePort) {
      return messagePort;
    }

    const options: WorkerOptions = {type: 'module'};
    const worker = new SharedWorker(
        getSanitizedScriptUrl(WORKER_SCRIPT) as unknown as string, options);
    worker.onerror = () => {
      console.warn('Error to initialize the ContentMetadataProvider');
    };
    return worker.port;
  }

  /**
   * Converts content metadata from parsers to the internal format.
   * @param metadata The content metadata.
   * @return Converted metadata.
   */
  static convertContentMetadata(metadata: ParserMetadata): MetadataItem {
    const item = new MetadataItem();
    item.contentImageTransform = metadata['imageTransform'];
    item.contentThumbnailTransform = metadata['thumbnailTransform'];
    item.contentThumbnailUrl = metadata['thumbnailURL'];
    item.exifLittleEndian = metadata['littleEndian'];
    item.ifd = metadata['ifd'];
    item.imageHeight = metadata['height'];
    item.imageWidth = metadata['width'];
    item.mediaMimeType = metadata['mimeType'];
    return item;
  }

  override get(requests: MetadataRequest[]): Promise<MetadataItem[]> {
    if (!requests.length) {
      return Promise.resolve([]);
    }

    const promises = [];
    for (const request of requests) {
      promises.push(new Promise<MetadataItem>(fulfill => {
        this.getImpl_(request.entry, request.names, fulfill);
      }));
    }

    return Promise.all(promises);
  }

  /**
   * Fetches the entry metadata.
   * @param entry File entry.
   * @param names Requested metadata types.
   * @param callback MetadataItem callback. Note
   *     this callback is called asynchronously.
   */
  private getImpl_(
      entry: Entry|FilesAppEntry, names: string[],
      callback: (item: MetadataItem) => void) {
    if (entry.isDirectory) {
      const cause = 'Directories do not have a thumbnail.';
      const error = this.createError_(entry.toURL(), 'get', cause);
      setTimeout(callback, 0, error);
      return;
    }

    const type = getType(entry);

    if (type && type.type === 'image') {
      // Parse the image using the Worker image metadata parsers.
      const url = entry.toURL();
      const urlCallbacks = this.callbacks_[url];
      if (urlCallbacks) {
        urlCallbacks.push(callback);
      } else {
        this.callbacks_[url] = [callback];
        this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
      }
      return;
    }

    if (type && type.type === 'raw' && names.includes('ifd')) {
      // The RAW file ifd will be processed herein, so remove ifd from names.
      names.splice(names.indexOf('ifd'), 1);

      /**
       * Creates an ifdError metadata item: when reading the fileEntry failed
       * or extracting its ifd data failed.
       */
      function createIfdError(
          fileEntry: Entry|FilesAppEntry, error: string): MetadataItem {
        const url = fileEntry.toURL();
        const step = 'read file entry';
        const item = new MetadataItem();
        item.ifdError = new ContentMetadataProviderError(url, step, error);
        return item;
      }

      new Promise<LoadImageResponse>((resolve, reject) => {
        (entry as FileEntry)
            .file(
                file => {
                  const request = createForUrl(entry.toURL());
                  request.maxWidth = THUMBNAIL_MAX_WIDTH;
                  request.maxHeight = THUMBNAIL_MAX_HEIGHT;
                  request.timestamp = file.lastModified;
                  request.cache = true;
                  request.priority = 0;
                  ImageLoaderClient.getInstance().load(request, resolve);
                },
                error => {
                  callback(createIfdError(entry, error.toString()));
                  reject();
                });
      }).then(result => {
        if (result.status === LoadImageResponseStatus.SUCCESS) {
          const item = new MetadataItem();
          if (result.ifd) {
            item.ifd = JSON.parse(result.ifd);
          }
          callback(item);
        } else {
          callback(createIfdError(entry, 'raw file has no ifd data'));
        }
      });

      if (!names.length) {
        return;
      }
    }

    const fileEntry = unwrapEntry(entry) as FileEntry;
    this.getContentMetadata_(fileEntry, names).then(callback);
  }

  /**
   * Gets the content metadata for a file entry consisting of the content mime
   * type. For audio and video file content mime types, additional metadata is
   * extracted if requested, such as metadata tags and images.
   *
   * @param entry File entry.
   * @param names Requested metadata types.
   * @return Promise that resolves with the content
   *     metadata of the file entry.
   */
  private async getContentMetadata_(entry: FileEntry, names: string[]):
      Promise<MetadataItem> {
    /**
     * First step is to determine the sniffed content mime type of |entry|.
     */
    const getMetadataItem = async(): Promise<MetadataItem> => {
      try {
        const mimeType = await getContentMimeType(entry);
        const item = new MetadataItem();
        item.contentMimeType = mimeType;
        item.mediaMimeType = mimeType;
        return item;
      } catch (error: any) {
        return this.createError_(entry.toURL(), 'sniff mime type', error);
      }
    };

    /**
     * Once the content mime type sniff step is done, search |names| for any
     * remaining media metadata to extract from the file. Note mediaMimeType
     * is excluded since it is used for the sniff step.
     * @param names Requested metadata types.
     * @param type File entry content mime type.
     * @return Media metadata type: false for metadata tags, true
     *    for metadata tags and images. A null return means there is no more
     *    media metadata that needs to be extracted.
     */
    function getMediaMetadataType(
        names: string[], type: (string|undefined)): null|boolean {
      if (!type || !names.length) {
        return null;
      } else if (!type.startsWith('audio/') && !type.startsWith('video/')) {
        return null;  // Only audio and video are supported.
      } else if (names.includes('contentThumbnailUrl')) {
        return true;  // Metadata tags and images.
      } else if (names.find(
                     (name) => name.startsWith('media') &&
                         name !== 'mediaMimeType')) {
        return false;  // Metadata tags only.
      }
      return null;
    }

    const item = await getMetadataItem();
    const extract = getMediaMetadataType(names, item.contentMimeType);
    if (extract === null) {
      return item;  // done: no more media metadata to extract.
    }
    try {
      const contentMimeType = item.contentMimeType;
      assert(contentMimeType);
      const metadata =
          await getContentMetadata(entry, contentMimeType, !!extract);
      return await this.convertMediaMetadataToMetadataItem_(entry, metadata);
    } catch (error: any) {
      return this.createError_(entry.toURL(), 'content metadata', error);
    }
  }

  /**
   * Dispatches a message from a metadata reader to the appropriate on* method.
   * @param event The event.
   */
  private onMessage_(event: {data: {verb: string, arguments: unknown[]}}) {
    const data = event.data;
    switch (data.verb) {
      case 'initialized':
        this.onInitialized_();
        break;
      case 'result':
        const [fileURL, metadata] = data.arguments as [string, ParserMetadata];
        this.onResult_(
            fileURL,
            metadata ?
                ContentMetadataProvider.convertContentMetadata(metadata) :
                new MetadataItem());
        break;
      case 'error':
        const [url, step, cause] = data.arguments as [string, string, string];
        const error = this.createError_(url, step, cause);
        this.onResult_(url, error);
        break;
      case 'log':
        const [message] = data.arguments as [string, ];
        this.onLog_(message);
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Handles the 'initialized' message from the metadata Worker.
   */
  private onInitialized_() {
    // Tests can monitor for this state with
    // ExtensionTestMessageListener listener("worker-initialized");
    // ASSERT_TRUE(listener.WaitUntilSatisfied());
    // Automated tests need to wait for this, otherwise we crash in
    // browser_test cleanup because the worker process still has
    // URL requests in-flight.
    testSendMessage('worker-initialized');
  }

  /**
   * Handles the 'result' message from the metadata Worker.
   * @param url File url.
   * @param metadataItem The metadata item.
   */
  private onResult_(url: string, metadataItem: MetadataItem) {
    const callbacks = this.callbacks_[url]!;
    delete this.callbacks_[url];
    for (const callback of callbacks) {
      callback(metadataItem);
    }
  }

  /**
   * Handles the 'log' message from the metadata Worker.
   * @param arglist Log arguments.
   */
  private onLog_(message: string) {
    console.info('ContentMetadataProvider log:' + message);
  }

  /**
   * Converts fileManagerPrivate.MediaMetadata |metadata| to a MetadataItem.
   * @param entry File entry.
   * @param metadata The metadata.
   * @return Promise that resolves with the
   *    converted metadata item.
   */
  private convertMediaMetadataToMetadataItem_(
      entry: FileEntry, metadata: chrome.fileManagerPrivate.MediaMetadata):
      Promise<MetadataItem> {
    return new Promise((resolve) => {
      if (!metadata) {
        resolve(this.createError_(
            entry.toURL(), 'metadata result', 'Failed to parse metadata'));
        return;
      }

      const item = new MetadataItem();
      const mimeType = metadata['mimeType'];
      item.contentMimeType = mimeType;
      item.mediaMimeType = mimeType;

      const trans = {scaleX: 1, scaleY: 1, rotate90: 0};
      if (metadata.rotation) {
        switch (metadata.rotation) {
          case 0:
            break;
          case 90:
            trans.rotate90 = 1;
            break;
          case 180:
            trans.scaleX *= -1;
            trans.scaleY *= -1;
            break;
          case 270:
            trans.rotate90 = 1;
            trans.scaleX *= -1;
            trans.scaleY *= -1;
            break;
          default:
            console.error('Unknown rotation angle: ', metadata.rotation);
        }
      }
      if (metadata.rotation) {
        item.contentImageTransform = item.contentThumbnailTransform = trans;
      }

      item.imageHeight = metadata['height'];
      item.imageWidth = metadata['width'];
      item.mediaAlbum = metadata['album'];
      item.mediaArtist = metadata['artist'];
      item.mediaDuration = metadata['duration'];
      item.mediaGenre = metadata['genre'];
      item.mediaTitle = metadata['title'];

      if (metadata['track']) {
        item.mediaTrack = '' + metadata['track'];
      }
      if (metadata.rawTags) {
        metadata.rawTags.forEach(entry => {
          const tags = entry.tags as Record<string, string>;
          if (entry.type === 'mp3') {
            if (tags['date']) {
              item.mediaYearRecorded = tags['date'];
            }
            // It is possible that metadata['track'] is undefined but this is
            // defined.
            if (tags['track']) {
              item.mediaTrack = tags['track'];
            }
          }
        });
      }

      if (metadata.attachedImages && metadata.attachedImages.length > 0) {
        item.contentThumbnailUrl = metadata.attachedImages[0]!.data;
      }

      resolve(item);
    });
  }

  /**
   * Returns an 'error' MetadataItem.
   * @param url File entry.
   * @param step Step that failed.
   * @param cause Error cause.
   * @return Error metadata
   */
  private createError_(url: string, step: string, cause: string): MetadataItem {
    const error = new ContentMetadataProviderError(url, step, cause);
    const item = new MetadataItem();
    item.contentImageTransformError = error;
    item.contentThumbnailTransformError = error;
    item.contentThumbnailUrlError = error;
    return item;
  }
}

class ContentMetadataProviderError extends Error {
  /**
   * @param url File Entry.
   * @param step Step that failed.
   * @param errorDescription Error cause.
   */
  constructor(
      public readonly url: string, public readonly step: string,
      public readonly errorDescription: string) {
    super(errorDescription);
  }
}
