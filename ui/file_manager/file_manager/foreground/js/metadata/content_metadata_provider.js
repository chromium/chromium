// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @final */
class ContentMetadataProvider extends MetadataProvider {
  /**
   * @param {!MessagePort=} opt_messagePort Message port overriding the default
   *     worker port.
   */
  constructor(opt_messagePort) {
    super(ContentMetadataProvider.PROPERTY_NAMES);

    /**
     * Pass all URLs to the metadata reader until we have a correct filter.
     * @private {RegExp}
     */
    this.urlFilter_ = /.*/;

    /** @private @const {!MessagePort} */
    this.dispatcher_ = opt_messagePort ?
        opt_messagePort :
        new SharedWorker(ContentMetadataProvider.WORKER_SCRIPT).port;
    this.dispatcher_.onmessage = this.onMessage_.bind(this);
    this.dispatcher_.postMessage({verb: 'init'});
    this.dispatcher_.start();

    /**
     * Initialization is not complete until the Worker sends back the
     * 'initialized' message.  See below.
     * @private {boolean}
     */
    this.initialized_ = false;

    /**
     * Map from Entry.toURL() to callback.
     * Note that simultaneous requests for same url are handled in
     * MetadataCache.
     * @private @const {!Object<!string, !Array<function(!MetadataItem)>>}
     */
    this.callbacks_ = {};
  }

  /**
   * Converts content metadata from parsers to the internal format.
   * @param {Object} metadata The content metadata.
   * @return {!MetadataItem} Converted metadata.
   */
  static convertContentMetadata(metadata) {
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

  /** @override */
  get(requests) {
    if (!requests.length) {
      return Promise.resolve([]);
    }

    const promises = [];
    for (let i = 0; i < requests.length; i++) {
      promises.push(new Promise(((request, fulfill) => {
                                  this.getImpl_(
                                      request.entry, request.names, fulfill);
                                }).bind(null, requests[i])));
    }
    return Promise.all(promises);
  }

  /**
   * Fetches the metadata.
   * @param {!Entry} entry File entry.
   * @param {!Array<string>} names Requested metadata type.
   * @param {function(!MetadataItem)} callback Callback expects metadata value.
   *     This callback is called asynchronously.
   * @private
   */
  getImpl_(entry, names, callback) {
    if (entry.isDirectory) {
      const cause = 'Directories do not have a thumbnail.';
      const error = this.createError_(entry.toURL(), 'get', cause);
      setTimeout(callback, 0, error);
      return;
    }

    const type = FileType.getType(entry);

    // TODO(ryoh): mediaGalleries API does not handle image metadata correctly.
    // We parse it in our pure js parser.
    // chrome/browser/media_galleries/fileapi/supported_image_type_validator.cc
    if (type && type.type === 'image') {
      const url = entry.toURL();
      if (this.callbacks_[url]) {
        this.callbacks_[url].push(callback);
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
       * @param {!Entry} fileEntry
       * @param {string} error
       * @return {!MetadataItem}
       */
      function createIfdError(fileEntry, error) {
        const url = fileEntry.toURL();
        const step = 'read file entry';
        const item = new MetadataItem();
        item.ifdError = new ContentMetadataProvider.Error(url, step, error);
        return item;
      }

      new Promise((resolve, reject) => {
        entry.file(
            file => {
              const request = LoadImageRequest.createForUrl(entry.toURL());
              request.maxWidth = ThumbnailLoader.THUMBNAIL_MAX_WIDTH;
              request.maxHeight = ThumbnailLoader.THUMBNAIL_MAX_HEIGHT;
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

    this.getFromMediaGalleries_(entry, names).then(callback);
  }

  /**
   * Gets a metadata from mediaGalleries API
   *
   * @param {!Entry} entry File entry.
   * @param {!Array<string>} names Requested metadata type.
   * @return {!Promise<!MetadataItem>}  Promise that resolves with the metadata
   *     of
   *    the entry.
   * @private
   */
  getFromMediaGalleries_(entry, names) {
    const self = this;
    return new Promise((resolve, reject) => {
      entry.file(
          blob => {
            let metadataType = 'mimeTypeOnly';
            if (names.indexOf('mediaArtist') !== -1 ||
                names.indexOf('mediaTitle') !== -1 ||
                names.indexOf('mediaTrack') !== -1 ||
                names.indexOf('mediaYearRecorded') !== -1) {
              metadataType = 'mimeTypeAndTags';
            }
            if (names.indexOf('contentThumbnailUrl') !== -1) {
              metadataType = 'all';
            }
            chrome.mediaGalleries.getMetadata(
                blob, {metadataType: metadataType}, metadata => {
                  if (chrome.runtime.lastError) {
                    resolve(self.createError_(
                        entry.toURL(), 'resolving metadata',
                        chrome.runtime.lastError.toString()));
                  } else {
                    self.convertMediaMetadataToMetadataItem_(entry, metadata)
                        .then(resolve, reject);
                  }
                });
          },
          err => {
            resolve(self.createError_(
                entry.toURL(), 'loading file entry',
                'failed to open file entry'));
          });
    });
  }

  /**
   * Dispatches a message from a metadata reader to the appropriate on* method.
   * @param {Object} event The event.
   * @private
   */
  onMessage_(event) {
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
   * Handles the 'initialized' message from the metadata reader Worker.
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
    util.testSendMessage('worker-initialized');
    this.initialized_ = true;
  }

  /**
   * Handles the 'result' message from the worker.
   * @param {string} url File url.
   * @param {!MetadataItem} metadataItem The metadata item.
   * @private
   */
  onResult_(url, metadataItem) {
    const callbacks = this.callbacks_[url];
    delete this.callbacks_[url];
    for (let i = 0; i < callbacks.length; i++) {
      callbacks[i](metadataItem);
    }
  }

  /**
   * Handles the 'log' message from the worker.
   * @param {Array<*>} arglist Log arguments.
   * @private
   */
  onLog_(arglist) {
    console.log.apply(
        console, ['ContentMetadataProvider log:'].concat(arglist));
  }

  /**
   * Dispatches a message from MediaGalleries API to the appropriate on* method.
   * @param {!Entry} entry File entry.
   * @param {!Object} metadata The metadata from MediaGalleries API.
   * @return {!Promise<!MetadataItem>}  Promise that resolves with
   *    converted metadata item.
   * @private
   */
  convertMediaMetadataToMetadataItem_(entry, metadata) {
    return new Promise((resolve, reject) => {
      if (!metadata) {
        resolve(this.createError_(
            entry.toURL(), 'Reading a thumbnail image',
            'Failed to parse metadata'));
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
            if (entry.tags['date']) {
              item.mediaYearRecorded = entry.tags['date'];
            }
            // It is possible that metadata['track'] is undefined but this is
            // defined.
            if (entry.tags['track']) {
              item.mediaTrack = entry.tags['track'];
            }
          }
        });
      }
      if (metadata.attachedImages && metadata.attachedImages.length > 0) {
        const reader = new FileReader();
        reader.onload = e => {
          item.contentThumbnailUrl = e.target.result;
          resolve(item);
        };
        reader.onerror = e => {
          resolve(this.createError_(
              entry.toURL(), 'Reading a thumbnail image',
              reader.error.toString()));
        };
        reader.readAsDataURL(metadata.attachedImages[0]);
      } else {
        resolve(item);
      }
    });
  }

  /**
   * Handles the 'error' message from the worker.
   * @param {string} url File entry.
   * @param {string} step Step failed.
   * @param {string} errorDescription Error description.
   * @return {!MetadataItem} Error metadata
   * @private
   */
  createError_(url, step, errorDescription) {
    // For error case, fill all fields with error object.
    const error =
        new ContentMetadataProvider.Error(url, step, errorDescription);
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
   * @param {string} step Step failed.
   * @param {string} errorDescription Error description.
   */
  constructor(url, step, errorDescription) {
    super(errorDescription);

    /** @public @const {string} */
    this.url = url;

    /** @public @const {string} */
    this.step = step;

    /** @public @const {string} */
    this.errorDescription = errorDescription;
  }
};

/** @public @const {!Array<string>} */
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
 * Path of a worker script.
 * @public @const {string}
 */
ContentMetadataProvider.WORKER_SCRIPT =
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
    'foreground/js/metadata/metadata_dispatcher.js';
