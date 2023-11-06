// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import {LoadImageRequest, LoadImageResponseStatus} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {unwrapEntry} from '../../../common/js/entry_utils.js';
import {FileType} from '../../../common/js/file_type.js';
import {getSanitizedScriptUrl} from '../../../common/js/trusted_script_url_policy_util.js';
import {testSendMessage} from '../../../common/js/util.js';
import {ThumbnailLoader} from '../thumbnail_loader.js';

import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';

/** @final */
export class ContentMetadataProvider extends MetadataProvider {
  /**
   * @param {!MessagePort=} opt_messagePort Message port overriding the default
   *     worker port.
   */
  constructor(opt_messagePort) {
    super(ContentMetadataProvider.PROPERTY_NAMES);

    /**
     * Pass all URLs to the metadata reader until we have a correct filter.
     * @private @type {RegExp}
     */
    this.urlFilter_ = /.*/;

    /**
     * Initialization is not complete until the Worker sends back the
     * 'initialized' message.  See below.
     * @private @type {boolean}
     */
    this.initialized_ = false;

    /**
     * Map from Entry.toURL() to callback.
     * Note that simultaneous requests for same url are handled in
     * MetadataCache.
     * @private @const @type {!Record<!string,
     *     !Array<function(!MetadataItem):void>>}
     */
    this.callbacks_ = {};

    /**
     * Setup |this.disapatcher_|. Creates the Shared Worker if needed.
     * @private @const @type {!MessagePort}
     */
    this.dispatcher_ = this.createSharedWorker_(opt_messagePort);
    this.dispatcher_.onmessage = this.onMessage_.bind(this);
    this.dispatcher_.onmessageerror = (error) => {
      console.warn('ContentMetadataProvider worker msg error:', error);
    };
    this.dispatcher_.postMessage({verb: 'init'});
    this.dispatcher_.start();
  }

  /**
   * Returns |opt_messagePort| if given. Otherwise creates the Shared Worker
   * and returns its message port.
   * @param {!MessagePort=} opt_messagePort
   * @private
   * @return {!MessagePort}
   * @suppress {checkTypes}: crbug.com/1150718
   */
  createSharedWorker_(opt_messagePort) {
    if (opt_messagePort) {
      return opt_messagePort;
    }

    const script = ContentMetadataProvider.getWorkerScript();

    /** @type {!WorkerOptions} */
    const options =
        ContentMetadataProvider.loadAsModule_ ? {type: 'module'} : {};

    // @ts-ignore: error TS2345: Argument of type 'TrustedScriptURL' is not
    // assignable to parameter of type 'string | URL'.
    const worker = new SharedWorker(getSanitizedScriptUrl(script), options);
    worker.onerror = () => {
      console.warn(
          'Error to initialize the ContentMetadataProvider ' +
          'SharedWorker: ' + script);
    };
    return worker.port;
  }

  /**
   * Configures how the worker should be loaded.
   *
   * @param {string} scriptURL URL used to load the worker.
   * @param {boolean=} isModule Indicate if the worker should be loaded as
   *   module.
   */
  static configure(scriptURL, isModule) {
    ContentMetadataProvider.workerScript_ = scriptURL;
    ContentMetadataProvider.loadAsModule_ = !!isModule;
  }

  /** @public @return {string} */
  static getWorkerScript() {
    return ContentMetadataProvider.workerScript_ ?
        ContentMetadataProvider.workerScript_ :
        ContentMetadataProvider.DEFAULT_WORKER_SCRIPT_;
  }

  /**
   * Converts content metadata from parsers to the internal format.
   * @param {Object} metadata The content metadata.
   * @return {!MetadataItem} Converted metadata.
   */
  static convertContentMetadata(metadata) {
    const item = new MetadataItem();
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"imageTransform"' can't be used to index type
    // 'Object'.
    item.contentImageTransform = metadata['imageTransform'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"thumbnailTransform"' can't be used to index type
    // 'Object'.
    item.contentThumbnailTransform = metadata['thumbnailTransform'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"thumbnailURL"' can't be used to index type 'Object'.
    item.contentThumbnailUrl = metadata['thumbnailURL'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"littleEndian"' can't be used to index type 'Object'.
    item.exifLittleEndian = metadata['littleEndian'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"ifd"' can't be used to index type 'Object'.
    item.ifd = metadata['ifd'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"height"' can't be used to index type 'Object'.
    item.imageHeight = metadata['height'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"width"' can't be used to index type 'Object'.
    item.imageWidth = metadata['width'];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type '"mimeType"' can't be used to index type 'Object'.
    item.mediaMimeType = metadata['mimeType'];
    return item;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'requests' implicitly has an 'any'
  // type.
  get(requests) {
    if (!requests.length) {
      return Promise.resolve([]);
    }

    const promises = [];
    for (let i = 0; i < requests.length; i++) {
      // @ts-ignore: error TS7006: Parameter 'fulfill' implicitly has an 'any'
      // type.
      promises.push(new Promise(function(request, fulfill) {
        // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
        // does not have a type annotation.
        this.getImpl_(request.entry, request.names, fulfill);
      }.bind(this, requests[i])));
    }

    return Promise.all(promises);
  }

  /**
   * Fetches the entry metadata.
   * @param {!Entry} entry File entry.
   * @param {!Array<string>} names Requested metadata types.
   * @param {function(!MetadataItem):void} callback MetadataItem callback. Note
   *     this callback is called asynchronously.
   * @private
   */
  // @ts-ignore: error TS6133: 'getImpl_' is declared but its value is never
  // read.
  getImpl_(entry, names, callback) {
    if (entry.isDirectory) {
      const cause = 'Directories do not have a thumbnail.';
      const error = this.createError_(entry.toURL(), 'get', cause);
      setTimeout(callback, 0, error);
      return;
    }

    const type = FileType.getType(entry);

    if (type && type.type === 'image') {
      // Parse the image using the Worker image metadata parsers.
      const url = entry.toURL();
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      if (this.callbacks_[url]) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        this.callbacks_[url].push(callback);
      } else {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
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
       * @param {!Entry} fileEntry
       * @param {string} error
       * @return {!MetadataItem}
       */
      function createIfdError(fileEntry, error) {
        const url = fileEntry.toURL();
        const step = 'read file entry';
        const item = new MetadataItem();
        // @ts-ignore: error TS2339: Property 'ifdError' does not exist on type
        // 'MetadataItem'.
        item.ifdError = new ContentMetadataProvider.Error(url, step, error);
        return item;
      }

      new Promise((resolve, reject) => {
        // @ts-ignore: error TS2339: Property 'file' does not exist on type
        // 'FileSystemEntry'.
        entry.file(
            // @ts-ignore: error TS7006: Parameter 'file' implicitly has an
            // 'any' type.
            file => {
              const request = LoadImageRequest.createForUrl(entry.toURL());
              request.maxWidth = ThumbnailLoader.THUMBNAIL_MAX_WIDTH;
              request.maxHeight = ThumbnailLoader.THUMBNAIL_MAX_HEIGHT;
              request.timestamp = file.lastModified;
              request.cache = true;
              request.priority = 0;
              ImageLoaderClient.getInstance().load(request, resolve);
            },
            // @ts-ignore: error TS7006: Parameter 'error' implicitly has an
            // 'any' type.
            error => {
              callback(createIfdError(entry, error.toString()));
              reject();
            });
      }).then(result => {
        if (result.status === LoadImageResponseStatus.SUCCESS) {
          const item = new MetadataItem();
          if (result.ifd) {
            item.ifd = /** @type {!Object} */ (JSON.parse(result.ifd));
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

    const fileEntry = /** @type {!FileEntry} */ (unwrapEntry(entry));
    this.getContentMetadata_(fileEntry, names).then(callback);
  }

  /**
   * Gets the content metadata for a file entry consisting of the content mime
   * type. For audio and video file content mime types, additional metadata is
   * extacted if requested, such as metadata tags and images.
   *
   * @param {!FileEntry} entry File entry.
   * @param {!Array<string>} names Requested metadata types.
   * @return {!Promise<!MetadataItem>} Promise that resolves with the content
   *     metadata of the file entry.
   * @private
   */
  getContentMetadata_(entry, names) {
    /**
     * First step is to determine the sniffed content mime type of |entry|.
     * @const @type {!Promise<!MetadataItem>}
     */
    // @ts-ignore: error TS6133: 'reject' is declared but its value is never
    // read.
    const getContentMimeType = new Promise((resolve, reject) => {
      chrome.fileManagerPrivate.getContentMimeType(entry, resolve);
    }).then(mimeType => {
      if (chrome.runtime.lastError) {
        const error = chrome.runtime.lastError.toString();
        return this.createError_(entry.toURL(), 'sniff mime type', error);
      }
      const item = new MetadataItem();
      item.contentMimeType = mimeType;
      item.mediaMimeType = mimeType;
      return item;
    });

    /**
     * Once the content mime type sniff step is done, search |names| for any
     * remaining media metadata to extract from the file. Note mediaMimeType
     * is excluded since it is used for the sniff step.
     * @param {!Array<string>} names Requested metadata types.
     * @param {(string|undefined)} type File entry content mime type.
     * @return {?boolean} Media metadata type: false for metadata tags, true
     *    for metadata tags and images. A null return means there is no more
     *    media metadata that needs to be extracted.
     */
    function getMediaMetadataType(names, type) {
      if (!type || !names.length) {
        return null;
      } else if (!type.startsWith('audio/') && !type.startsWith('video/')) {
        return null;  // Only audio and video are supported.
      } else if (names.includes('contentThumbnailUrl')) {
        return true;  // Metadata tags and images.
      } else if (names.find((name) =>
          name.startsWith('media') && name !== 'mediaMimeType')) {
        return false;  // Metadata tags only.
      }
      return null;
    }

    return getContentMimeType.then(item => {
      const extract = getMediaMetadataType(names, item.contentMimeType);
      if (extract === null) {
        return item;  // done: no more media metadata to extract.
      }
      // @ts-ignore: error TS6133: 'reject' is declared but its value is never
      // read.
      return new Promise((resolve, reject) => {
        const contentMimeType = assert(item.contentMimeType);
        chrome.fileManagerPrivate.getContentMetadata(
            // @ts-ignore: error TS2345: Argument of type 'string | undefined'
            // is not assignable to parameter of type 'string'.
            entry, contentMimeType, !!extract, resolve);
      }).then(metadata => {
        if (chrome.runtime.lastError) {
          const error = chrome.runtime.lastError.toString();
          return this.createError_(entry.toURL(), 'content metadata', error);
        }
        return this.convertMediaMetadataToMetadataItem_(entry, metadata);
      }).catch((_, error = 'Conversion failed') => {
        return this.createError_(entry.toURL(), 'convert metadata', error);
      });
    });
  }

  /**
   * Dispatches a message from a metadata reader to the appropriate on* method.
   * @param {Object} event The event.
   * @private
   */
  onMessage_(event) {
    // @ts-ignore: error TS2339: Property 'data' does not exist on type
    // 'Object'.
    const data = event.data;
    switch (data.verb) {
      case 'initialized':
        this.onInitialized_(data.arguments[0]);
        break;
      case 'result':
        this.onResult_(
            data.arguments[0],
            data.arguments[1] ? ContentMetadataProvider.convertContentMetadata(
                                    data.arguments[1]) :
                                new MetadataItem());
        break;
      case 'error':
        const error = this.createError_(
            data.arguments[0], data.arguments[1], data.arguments[2]);
        this.onResult_(data.arguments[0], error);
        break;
      case 'log':
        this.onLog_(data.arguments[0]);
        break;
      default:
        assertNotReached();
        break;
    }
  }

  /**
   * Handles the 'initialized' message from the metadata Worker.
   * @param {RegExp} regexp Regexp of supported urls.
   * @private
   */
  onInitialized_(regexp) {
    this.urlFilter_ = regexp;

    // Tests can monitor for this state with
    // ExtensionTestMessageListener listener("worker-initialized");
    // ASSERT_TRUE(listener.WaitUntilSatisfied());
    // Automated tests need to wait for this, otherwise we crash in
    // browser_test cleanup because the worker process still has
    // URL requests in-flight.
    testSendMessage('worker-initialized');
    this.initialized_ = true;
  }

  /**
   * Handles the 'result' message from the metadata Worker.
   * @param {string} url File url.
   * @param {!MetadataItem} metadataItem The metadata item.
   * @private
   */
  onResult_(url, metadataItem) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    const callbacks = this.callbacks_[url];
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    delete this.callbacks_[url];
    // @ts-ignore: error TS18048: 'callbacks' is possibly 'undefined'.
    for (let i = 0; i < callbacks.length; i++) {
      // @ts-ignore: error TS2722: Cannot invoke an object which is possibly
      // 'undefined'.
      callbacks[i](metadataItem);
    }
  }

  /**
   * Handles the 'log' message from the metadata Worker.
   * @param {Array<*>} arglist Log arguments.
   * @private
   */
  onLog_(arglist) {
    console.log.apply(
        console, ['ContentMetadataProvider log:'].concat(arglist));
  }

  /**
   * Converts fileManagerPrivate.MediaMetadata |metadata| to a MetadataItem.
   * @param {!FileEntry} entry File entry.
   * @param {!chrome.fileManagerPrivate.MediaMetadata} metadata The metadata.
   * @return {!Promise<!MetadataItem>} Promise that resolves with the
   *    converted metadata item.
   * @private
   */
  convertMediaMetadataToMetadataItem_(entry, metadata) {
    // @ts-ignore: error TS6133: 'reject' is declared but its value is never
    // read.
    return new Promise((resolve, reject) => {
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
          if (entry.type === 'mp3') {
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type '"date"' can't be used to index type
            // 'Object'.
            if (entry.tags['date']) {
              // @ts-ignore: error TS7053: Element implicitly has an 'any' type
              // because expression of type '"date"' can't be used to index type
              // 'Object'.
              item.mediaYearRecorded = entry.tags['date'];
            }
            // It is possible that metadata['track'] is undefined but this is
            // defined.
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type '"track"' can't be used to index type
            // 'Object'.
            if (entry.tags['track']) {
              // @ts-ignore: error TS7053: Element implicitly has an 'any' type
              // because expression of type '"track"' can't be used to index
              // type 'Object'.
              item.mediaTrack = entry.tags['track'];
            }
          }
        });
      }

      if (metadata.attachedImages && metadata.attachedImages.length > 0) {
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        item.contentThumbnailUrl = metadata.attachedImages[0].data;
      }

      resolve(item);
    });
  }

  /**
   * Returns an 'error' MetadataItem.
   * @param {string} url File entry.
   * @param {string} step Step that failed.
   * @param {string} cause Error cause.
   * @return {!MetadataItem} Error metadata
   * @private
   */
  createError_(url, step, cause) {
    const error = new ContentMetadataProvider.Error(url, step, cause);
    const item = new MetadataItem();
    item.contentImageTransformError = error;
    item.contentThumbnailTransformError = error;
    item.contentThumbnailUrlError = error;
    return item;
  }
}

/**
 * Content metadata provider error.
 */
ContentMetadataProvider.Error = class extends Error {
  /**
   * @param {string} url File Entry.
   * @param {string} step Step that failed.
   * @param {string} cause Error cause.
   */
  constructor(url, step, cause) {
    super(cause);

    /** @public @const @type {string} */
    this.url = url;

    /** @public @const @type {string} */
    this.step = step;

    /** @public @const @type {string} */
    this.errorDescription = cause;
  }
};

/** @public @const @type {!Array<string>} */
ContentMetadataProvider.PROPERTY_NAMES = [
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
 * The metadata Worker script URL.
 * @const @private @type {string}
 */
// @ts-ignore: error TS2341: Property 'DEFAULT_WORKER_SCRIPT_' is private and
// only accessible within class 'ContentMetadataProvider'.
ContentMetadataProvider.DEFAULT_WORKER_SCRIPT_ =
    'foreground/js/metadata/metadata_dispatcher.js';

/**
 * Worker script URL that is overwritten by client code.
 * @private @type {?string}
 */
// @ts-ignore: error TS2341: Property 'workerScript_' is private and only
// accessible within class 'ContentMetadataProvider'.
ContentMetadataProvider.workerScript_ = null;

/**
 * Sets if the SharedWorker should start as a JS Module.
 * @private @type {boolean}
 */
// @ts-ignore: error TS2341: Property 'loadAsModule_' is private and only
// accessible within class 'ContentMetadataProvider'.
ContentMetadataProvider.loadAsModule_ = true;
