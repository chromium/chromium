// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFileTypeForName} from 'chrome://file-manager/common/js/file_types_base.js';
import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';

import {ImageCache} from './cache.js';
import {ImageLoaderUtil} from './image_loader_util.js';
import {ImageOrientation} from './image_orientation.js';
import {LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from './load_image_request.js';
import {PiexLoader} from './piex_loader.js';

/**
 * Creates and starts downloading and then resizing of the image. Finally,
 * returns the image using the callback.
 *
 * @param {string} id Request ID.
 * @param {ImageCache} cache Cache object.
 * @param {!LoadImageRequest} request Request message as a hash array.
 * @param {function(!LoadImageResponse)} callback Response handler.
 * @constructor
 */
export function ImageRequestTask(id, cache, request, callback) {
  /**
   * Global ID (concatenated client ID and client request ID).
   * @type {string}
   * @private
   */
  this.id_ = id;

  /**
   * @type {ImageCache}
   * @private
   */
  this.cache_ = cache;

  /**
   * @type {!LoadImageRequest}
   * @private
   */
  this.request_ = request;

  /**
   * @type {function(!LoadImageResponse)}
   * @private
   */
  this.sendResponse_ = callback;

  /**
   * Temporary image used to download images.
   * @type {Image}
   * @private
   */
  this.image_ = new Image();

  /**
   * MIME type of the fetched image.
   * @type {?string}
   * @private
   */
  this.contentType_ = null;

  /**
   * IFD data of the fetched image. Only RAW images provide a non-null
   * ifd at this time. Drive images might provide an ifd in future.
   * @type {?string}
   * @private
   */
  this.ifd_ = null;

  /**
   * Used to download remote images using http:// or https:// protocols.
   * @type {XMLHttpRequest}
   * @private
   */
  this.xhr_ = null;

  /**
   * Temporary canvas used to resize and compress the image.
   * @type {HTMLCanvasElement}
   * @private
   */
  this.canvas_ =
      /** @type {HTMLCanvasElement} */ (document.createElement('canvas'));

  /**
   * @type {CanvasRenderingContext2D}
   * @private
   */
  this.context_ =
      /** @type {CanvasRenderingContext2D} */ (this.canvas_.getContext('2d'));

  /**
   * @type {ImageOrientation|null}
   */
  this.renderOrientation_ = null;

  /**
   * Callback to be called once downloading is finished.
   * @type {?function()}
   * @private
   */
  this.downloadCallback_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.aborted_ = false;
}

/**
 * Seeks offset to generate video thumbnail.
 * TODO(ryoh):
 *   What is the best position for the thumbnail?
 *   The first frame seems not good -- sometimes it is a black frame.
 * @const
 * @type {number}
 */
ImageRequestTask.VIDEO_THUMBNAIL_POSITION = 3;  // [sec]

/**
 * The maximum milliseconds to load video. If loading video exceeds the limit,
 * we give up generating video thumbnail and free the consumed memory.
 * @const
 * @type {number}
 */
ImageRequestTask.MAX_MILLISECONDS_TO_LOAD_VIDEO = 3000;

/**
 * The default size (width and height) of a square thumbnail. The value is set
 * to match the behavior of drivefs thumbnail generation.
 * See chromeos/ash/components/drivefs/mojom/drivefs.mojom
 * @const
 * @type {number}
 */
ImageRequestTask.DEFAULT_THUMBNAIL_SQUARE_SIZE = 360;

/**
 * The default width of a non-square thumbnail. The value is set to match the
 * behavior of drivefs thumbnail generation.
 * See chromeos/ash/components/drivefs/mojom/drivefs.mojom
 * @const
 * @type {number}
 */
ImageRequestTask.DEFAULT_THUMBNAIL_WIDTH = 500;

/**
 * The default height of a non-square thumbnail. The value is set to match the
 * behavior of drivefs thumbnail generation.
 * See chromeos/ash/components/drivefs/mojom/drivefs.mojom
 * @const
 * @type {number}
 */
ImageRequestTask.DEFAULT_THUMBNAIL_HEIGHT = 500;

/**
 * A map which is used to estimate content type from extension.
 * @enum {string}
 */
ImageRequestTask.ExtensionContentTypeMap = {
  gif: 'image/gif',
  png: 'image/png',
  svg: 'image/svg',
  bmp: 'image/bmp',
  jpg: 'image/jpeg',
  jpeg: 'image/jpeg',
};

/**
 * Extracts MIME type of a data URL.
 * @param {string|undefined} dataUrl Data URL.
 * @return {?string} MIME type string, or null if the URL is invalid.
 */
ImageRequestTask.getDataUrlMimeType = function(dataUrl) {
  const dataUrlMatches = (dataUrl || '').match(/^data:([^,;]*)[,;]/);
  return dataUrlMatches ? dataUrlMatches[1] : null;
};

/**
 * Returns ID of the request.
 * @return {string} Request ID.
 */
ImageRequestTask.prototype.getId = function() {
  return this.id_;
};

/**
 * Returns the client's task ID for the request.
 * @return {number}
 */
ImageRequestTask.prototype.getClientTaskId = function() {
  // Every incoming request should have been given a taskId.
  assert(this.request_.taskId);
  return this.request_.taskId;
};

/**
 * Returns priority of the request. The higher priority, the faster it will
 * be handled. The highest priority is 0. The default one is 2.
 *
 * @return {number} Priority.
 */
ImageRequestTask.prototype.getPriority = function() {
  return (this.request_.priority !== undefined) ? this.request_.priority : 2;
};

/**
 * Tries to load the image from cache, if it exists in the cache, and sends
 * the response. Fails if the image is not found in the cache.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 */
ImageRequestTask.prototype.loadFromCacheAndProcess = function(
    onSuccess, onFailure) {
  this.loadFromCache_(
      function(width, height, ifd, data) {  // Found in cache.
        this.ifd_ = ifd;
        this.sendImageData_(width, height, data);
        onSuccess();
      }.bind(this),
      onFailure);  // Not found in cache.
};

/**
 * Tries to download the image, resizes and sends the response.
 *
 * @param {function()} callback Completion callback.
 */
ImageRequestTask.prototype.downloadAndProcess = function(callback) {
  if (this.downloadCallback_) {
    throw new Error('Downloading already started.');
  }

  this.downloadCallback_ = callback;
  this.downloadThumbnail_(
      this.onImageLoad_.bind(this), this.onImageError_.bind(this));
};

/**
 * Fetches the image from the persistent cache.
 *
 * @param {function(number, number, ?string, string)} onSuccess
 *    Success callback with the image width, height, ?ifd, and data.
 * @param {function()} onFailure Failure callback.
 * @private
 */
ImageRequestTask.prototype.loadFromCache_ = function(onSuccess, onFailure) {
  const cacheKey = LoadImageRequest.cacheKey(this.request_);

  if (!cacheKey) {
    // Cache key is not provided for the request.
    onFailure();
    return;
  }

  if (!this.request_.cache) {
    // Cache is disabled for this request; therefore, remove it from cache
    // if existed.
    this.cache_.removeImage(cacheKey);
    onFailure();
    return;
  }

  const timestamp = this.request_.timestamp;
  if (!timestamp) {
    // Persistent cache is available only when a timestamp is provided.
    onFailure();
    return;
  }

  this.cache_.loadImage(cacheKey, timestamp, onSuccess, onFailure);
};

/**
 * Saves the image to the persistent cache.
 *
 * @param {number} width Image width.
 * @param {number} height Image height.
 * @param {string} data Image data.
 * @private
 */
ImageRequestTask.prototype.saveToCache_ = function(width, height, data) {
  const timestamp = this.request_.timestamp;

  if (!this.request_.cache || !timestamp) {
    // Persistent cache is available only when a timestamp is provided.
    return;
  }

  const cacheKey = LoadImageRequest.cacheKey(this.request_);
  if (!cacheKey) {
    // Cache key is not provided for the request.
    return;
  }

  this.cache_.saveImage(cacheKey, timestamp, width, height, this.ifd_, data);
};

/**
 * Gets the target image size for external thumbnails, where supported.
   The defaults replicate drivefs thumbnailer behavior.
 * @return {{width: !number, height: !number}}
 */
ImageRequestTask.prototype.targetThumbnailSize_ = function() {
  const crop = !!this.request_.crop;
  const defaultWidth = crop ? ImageRequestTask.DEFAULT_THUMBNAIL_SQUARE_SIZE :
                              ImageRequestTask.DEFAULT_THUMBNAIL_WIDTH;
  const defaultHeight = crop ? ImageRequestTask.DEFAULT_THUMBNAIL_SQUARE_SIZE :
                               ImageRequestTask.DEFAULT_THUMBNAIL_HEIGHT;
  return {
    width: this.request_.width || defaultWidth,
    height: this.request_.height || defaultHeight,
  };
};

/**
 * Loads |this.image_| with the |this.request_.url| source or the thumbnail
 * image of the source.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
ImageRequestTask.prototype.downloadThumbnail_ = function(onSuccess, onFailure) {
  // Load methods below set |this.image_.src|. Call revokeObjectURL(src) to
  // release resources if the image src was created with createObjectURL().
  this.image_.onload = function() {
    URL.revokeObjectURL(this.image_.src);
    onSuccess();
  }.bind(this);
  this.image_.onerror = function() {
    URL.revokeObjectURL(this.image_.src);
    onFailure();
  }.bind(this);

  // Load dataURL sources directly.
  const dataUrlMimeType =
      ImageRequestTask.getDataUrlMimeType(this.request_.url);
  if (dataUrlMimeType) {
    this.image_.src = this.request_.url;
    this.contentType_ = dataUrlMimeType;
    return;
  }

  const resolveLocalFileSystemUrl = (url, onResolveSuccess) => {
    window.webkitResolveLocalFileSystemURL(url, onResolveSuccess, error => {
      console.warn(error);
      onFailure();
    });
  };

  const onExternalThumbnail = (dataUrl) => {
    if (chrome.runtime.lastError) {
      console.warn(chrome.runtime.lastError.message);
      onFailure();
    } else if (dataUrl) {
      this.image_.src = dataUrl;
      this.contentType_ = ImageRequestTask.getDataUrlMimeType(dataUrl);
    } else {
      onFailure();
    }
  };

  // Load Drive source thumbnail.
  const drivefsUrlMatches = this.request_.url.match(/^drivefs:(.*)/);
  if (drivefsUrlMatches) {
    const url = drivefsUrlMatches[1];
    const cropToSquare = !!this.request_.crop;
    resolveLocalFileSystemUrl(
        url,
        entry => chrome.fileManagerPrivate.getDriveThumbnail(
            entry, cropToSquare, onExternalThumbnail));
    return;
  }

  // Load PDF source thumbnail.
  if (this.request_.url.endsWith('.pdf')) {
    const {width, height} = this.targetThumbnailSize_();
    resolveLocalFileSystemUrl(
        this.request_.url,
        entry => chrome.fileManagerPrivate.getPdfThumbnail(
            entry, width, height, onExternalThumbnail));
    return;
  }

  // Load DocumentsProvider thumbnail, if supported.
  const isDocumentsProviderRequest = !!this.request_.url.match(RegExp(
      'filesystem:chrome-extension://[a-z]+/external/arc-documents-provider/.*'));
  if (isDocumentsProviderRequest) {
    const {width, height} = this.targetThumbnailSize_();
    resolveLocalFileSystemUrl(this.request_.url, entry => {
      chrome.fileManagerPrivate.getArcDocumentsProviderThumbnail(
          entry, width, height, onExternalThumbnail);
    });
    return;
  }

  const fileType = getFileTypeForName(this.request_.url);

  // Load RAW image source thumbnail.
  if (fileType.type === 'raw') {
    PiexLoader.load(this.request_.url, chrome.runtime.reload)
        .then(
            function(data) {
              this.renderOrientation_ =
                  ImageOrientation.fromExifOrientation(data.orientation);
              this.ifd_ = data.ifd;
              this.contentType_ = data.mimeType;
              const blob = new Blob([data.thumbnail], {type: data.mimeType});
              this.image_.src = URL.createObjectURL(blob);
            }.bind(this),
            function() {
              // PiexLoader calls console.warn on errors.
              onFailure();
            });
    return;
  }

  // Load video source thumbnail.
  if (fileType.type === 'video') {
    this.createVideoThumbnailUrl_(this.request_.url)
        .then(function(url) {
          this.image_.src = url;
        }.bind(this))
        .catch(function(error) {
          console.warn('Video thumbnail error: ', error);
          onFailure();
        });
    return;
  }

  // Load the source directly.
  this.load(this.request_.url, (contentType, blob) => {
    this.image_.src = blob ? URL.createObjectURL(blob) : '!';
    this.contentType_ = contentType || null;
    if (this.contentType_ === 'image/jpeg') {
      this.renderOrientation_ = ImageOrientation.fromExifOrientation(1);
    }
  }, onFailure);
};

/**
 * Creates a video thumbnail data url from video file.
 *
 * @param {string} url Video URL.
 * @return {!Promise<Blob>}  Promise that resolves with the data url of video
 *    thumbnail.
 * @private
 */
ImageRequestTask.prototype.createVideoThumbnailUrl_ = function(url) {
  const video =
      assertInstanceof(document.createElement('video'), HTMLVideoElement);
  return Promise
      .race([
        new Promise((resolve, reject) => {
          video.addEventListener('canplay', resolve);
          video.addEventListener('error', reject);
          video.currentTime = ImageRequestTask.VIDEO_THUMBNAIL_POSITION;
          video.preload = 'auto';
          video.src = url;
          video.load();
        }),
        new Promise((resolve) => {
          setTimeout(resolve, ImageRequestTask.MAX_MILLISECONDS_TO_LOAD_VIDEO);
        }).then(() => {
          // If we don't receive 'canplay' event after 3 seconds have passed for
          // some reason (e.g. unseekable video), we give up generating
          // thumbnail.
          video.src =
              '';  // Make sure to stop loading remaining part of the video.
          throw new Error('Seeking video failed.');
        }),
      ])
      .then(() => {
        const canvas = assertInstanceof(
            document.createElement('canvas'), HTMLCanvasElement);
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
        assertInstanceof(canvas.getContext('2d'), CanvasRenderingContext2D)
            .drawImage(video, 0, 0);
        return canvas.toDataURL();
      });
};

/**
 * Loads an image.
 *
 * @param {string} url URL to the resource to be fetched.
 * @param {function(string, Blob)} onSuccess Success callback with the content
 *     type and the fetched data.
 * @param {function()} onFailure Failure callback.
 */
ImageRequestTask.prototype.load = function(url, onSuccess, onFailure) {
  this.aborted_ = false;

  // Do not call any callbacks when aborting.
  const onMaybeSuccess =
      /** @type {function(string, Blob)} */ (function(contentType, response) {
        // When content type is not available, try to estimate it from url.
        if (!contentType) {
          contentType =
              ImageRequestTask
                  .ExtensionContentTypeMap[this.extractExtension_(url)];
        }

        if (!this.aborted_) {
          onSuccess(contentType, response);
        }
      }.bind(this));

  const onMaybeFailure = /** @type {function(number=)} */ (function(opt_code) {
    if (!this.aborted_) {
      onFailure();
    }
  }.bind(this));

  // The query parameter is workaround for crbug.com/379678, which forces the
  // browser to obtain the latest contents of the image.
  const noCacheUrl = url + '?nocache=' + Date.now();
  this.xhr_ =
      ImageRequestTask.load_(noCacheUrl, onMaybeSuccess, onMaybeFailure);
};

/**
 * Extracts extension from url.
 * @param {string} url Url.
 * @return {string} Extracted extension, e.g. png.
 */
ImageRequestTask.prototype.extractExtension_ = function(url) {
  const result = (/\.([a-zA-Z]+)$/i).exec(url);
  return result ? result[1] : '';
};

/**
 * Fetches data using XmlHttpRequest.
 *
 * @param {string} url URL to the resource to be fetched.
 * @param {function(string, Blob)} onSuccess Success callback with the content
 *     type and the fetched data.
 * @param {function(number=)} onFailure Failure callback with the error code
 *     if available.
 * @return {XMLHttpRequest} XHR instance.
 * @private
 */
ImageRequestTask.load_ = function(url, onSuccess, onFailure) {
  const xhr = new XMLHttpRequest();
  xhr.responseType = 'blob';

  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status != 200) {
      onFailure(xhr.status);
      return;
    }
    const response = /** @type {Blob} */ (xhr.response);
    const contentType = xhr.getResponseHeader('Content-Type') || response.type;
    onSuccess(contentType, response);
  }.bind(this);

  // Perform a xhr request.
  try {
    xhr.open('GET', url, true);
    xhr.send();
  } catch (e) {
    onFailure();
  }

  return xhr;
};

/**
 * Sends the resized image via the callback. If the image has been changed,
 * then packs the canvas contents, otherwise sends the raw image data.
 *
 * @param {boolean} imageChanged Whether the image has been changed.
 * @private
 */
ImageRequestTask.prototype.sendImage_ = function(imageChanged) {
  let width;
  let height;
  let data;

  if (!imageChanged) {
    // The image hasn't been processed, so the raw data can be directly
    // forwarded for speed (no need to encode the image again).
    width = this.image_.width;
    height = this.image_.height;
    data = this.image_.src;
  } else {
    // The image has been resized or rotated, therefore the canvas has to be
    // encoded to get the correct compressed image data.
    width = this.canvas_.width;
    height = this.canvas_.height;

    switch (this.contentType_) {
      case 'image/gif':
      case 'image/png':
      case 'image/svg':
      case 'image/bmp':
        data = this.canvas_.toDataURL('image/png');
        break;
      case 'image/jpeg':
      default:
        data = this.canvas_.toDataURL('image/jpeg', 0.9);
        break;
    }
  }

  // Send the image data and also save it in the persistent cache.
  this.sendImageData_(width, height, data);
  this.saveToCache_(width, height, data);
};

/**
 * Sends the resized image via the callback.
 *
 * @param {number} width Image width.
 * @param {number} height Image height.
 * @param {string} data Image data.
 * @private
 */
ImageRequestTask.prototype.sendImageData_ = function(width, height, data) {
  const result = {width, height, ifd: this.ifd_, data};
  this.sendResponse_(new LoadImageResponse(
      LoadImageResponseStatus.SUCCESS, this.getClientTaskId(), result));
};

/**
 * Handler, when contents are loaded into the image element. Performs image
 * processing operations if needed, and finalizes the request process.
 * @private
 */
ImageRequestTask.prototype.onImageLoad_ = function() {
  const requestOrientation = this.request_.orientation;

  // Override the request orientation before processing if needed.
  if (this.renderOrientation_) {
    this.request_.orientation = this.renderOrientation_;
  }

  // Perform processing if the url is not a data url, or if there are some
  // operations requested.
  let imageChanged = false;
  if (!(this.request_.url.match(/^data/) ||
        this.request_.url.match(/^drivefs:/)) ||
      ImageLoaderUtil.shouldProcess(
          this.image_.width, this.image_.height, this.request_)) {
    ImageLoaderUtil.resizeAndCrop(this.image_, this.canvas_, this.request_);
    imageChanged = true;  // The image is now on the <canvas>.
  }

  // Restore the request orientation after processing.
  if (this.renderOrientation_) {
    this.request_.orientation = requestOrientation;
  }

  // Finalize the request.
  this.sendImage_(imageChanged);
  this.cleanup_();
  this.downloadCallback_();
};

/**
 * Handler, when loading of the image fails. Sends a failure response and
 * finalizes the request process.
 * @private
 */
ImageRequestTask.prototype.onImageError_ = function() {
  this.sendResponse_(new LoadImageResponse(
      LoadImageResponseStatus.ERROR, this.getClientTaskId()));
  this.cleanup_();
  this.downloadCallback_();
};

/**
 * Cancels the request.
 */
ImageRequestTask.prototype.cancel = function() {
  this.cleanup_();

  // If downloading has started, then call the callback.
  if (this.downloadCallback_) {
    this.downloadCallback_();
  }
};

/**
 * Cleans up memory used by this request.
 * @private
 */
ImageRequestTask.prototype.cleanup_ = function() {
  this.image_.onerror = function() {};
  this.image_.onload = function() {};

  // Transparent 1x1 pixel gif, to force garbage collecting.
  this.image_.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAA' +
      'ABAAEAAAICTAEAOw==';

  this.aborted_ = true;
  if (this.xhr_) {
    this.xhr_.abort();
  }

  // Dispose memory allocated by Canvas.
  this.canvas_.width = 0;
  this.canvas_.height = 0;
};
