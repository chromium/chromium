// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates and starts downloading and then resizing of the image. Finally,
 * returns the image using the callback.
 *
 * @param {string} id Request ID.
 * @param {ImageCache} cache Cache object.
 * @param {!PiexLoader} piexLoader Piex loader for RAW file.
 * @param {!LoadImageRequest} request Request message as a hash array.
 * @param {function(!LoadImageResponse)} callback Response handler.
 * @constructor
 */
function ImageRequest(id, cache, piexLoader, request, callback) {
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
   * @type {!PiexLoader}
   * @private
   */
  this.piexLoader_ = piexLoader;

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
   * IFD data of the fetched image. Only RAW images provide non-null ifd
   * data at this time; images on Drive might provide ifd in future.
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
ImageRequest.VIDEO_THUMBNAIL_POSITION = 3; // [sec]

/**
 * The maximum milliseconds to load video. If loading video exceeds the limit,
 * we give up generating video thumbnail and free the consumed memory.
 * @const
 * @type {number}
 */
ImageRequest.MAX_MILLISECONDS_TO_LOAD_VIDEO = 3000;

/**
 * A map which is used to estimate content type from extension.
 * @enum {string}
 */
ImageRequest.ExtensionContentTypeMap = {
  gif: 'image/gif',
  png: 'image/png',
  svg: 'image/svg',
  bmp: 'image/bmp',
  jpg: 'image/jpeg',
  jpeg: 'image/jpeg'
};

/**
 * Returns ID of the request.
 * @return {string} Request ID.
 */
ImageRequest.prototype.getId = function() {
  return this.id_;
};

/**
 * Returns the client's task ID for the request.
 * @return {number}
 */
ImageRequest.prototype.getClientTaskId = function() {
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
ImageRequest.prototype.getPriority = function() {
  return (this.request_.priority !== undefined) ? this.request_.priority : 2;
};

/**
 * Tries to load the image from cache, if it exists in the cache, and sends
 * the response. Fails if the image is not found in the cache.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 */
ImageRequest.prototype.loadFromCacheAndProcess = function(
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
ImageRequest.prototype.downloadAndProcess = function(callback) {
  if (this.downloadCallback_) {
    throw new Error('Downloading already started.');
  }

  this.downloadCallback_ = callback;
  this.downloadOriginal_(this.onImageLoad_.bind(this),
                         this.onImageError_.bind(this));
};

/**
 * Fetches the image from the persistent cache.
 *
 * @param {function(number, number, ?string, string)} onSuccess
 *    Success callback with the image width, height, ?ifd, and data.
 * @param {function()} onFailure Failure callback.
 * @private
 */
ImageRequest.prototype.loadFromCache_ = function(onSuccess, onFailure) {
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
ImageRequest.prototype.saveToCache_ = function(width, height, data) {
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
 * Downloads an image directly or for remote resources using the XmlHttpRequest.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
ImageRequest.prototype.downloadOriginal_ = function(onSuccess, onFailure) {
  this.image_.onload = function() {
    URL.revokeObjectURL(this.image_.src);
    onSuccess();
  }.bind(this);
  this.image_.onerror = function() {
    URL.revokeObjectURL(this.image_.src);
    onFailure();
  }.bind(this);

  // Download data urls directly since they are not supported by XmlHttpRequest.
  var dataUrlMatches = this.request_.url.match(/^data:([^,;]*)[,;]/);
  if (dataUrlMatches) {
    this.image_.src = this.request_.url;
    this.contentType_ = dataUrlMatches[1];
    return;
  }
  var drivefsUrlMatches = this.request_.url.match(/^drivefs:(.*)/);
  if (drivefsUrlMatches) {
    window.webkitResolveLocalFileSystemURL(
        drivefsUrlMatches[1],
        entry => {
          chrome.fileManagerPrivate.getThumbnail(
              entry, !!this.request_.crop, thumbnail => {
                if (!thumbnail) {
                  onFailure();
                  return;
                }
                this.image_.src = thumbnail;
                this.contentType_ = 'image/png';
              });
        },
        error => {
          onFailure();
        });
    return;
  }

  var fileType = FileType.getTypeForName(this.request_.url);

  // Load RAW images by using Piex loader instead of XHR.
  if (fileType.type === 'raw') {
    this.piexLoader_.load(this.request_.url).then(function(data) {
      var blob = new Blob([data.thumbnail], {type: 'image/jpeg'});
      var url = URL.createObjectURL(blob);
      this.image_.src = url;
      this.request_.orientation = data.orientation;
      this.request_.colorSpace = data.colorSpace;
      this.ifd_ = data.ifd;
    }.bind(this), function() {
      // The error has already been logged in PiexLoader.
      onFailure();
    });
    return;
  }

  // Load video thumbnails by using video tag instead of XHR.
  if (fileType.type === 'video') {
    this.createVideoThumbnailUrl_(this.request_.url).then(function(url) {
      this.image_.src = url;
    }.bind(this)).catch(function(error) {
      console.error('Video thumbnail error: ', error);
      onFailure();
    });
    return;
  }

  // Fetch the image via XHR and parse it.
  var parseImage = function(contentType, blob) {
    if (contentType) {
      this.contentType_ = contentType;
    }
    this.image_.src = URL.createObjectURL(blob);
  }.bind(this);

  // Request raw data via XHR.
  this.load(this.request_.url, parseImage, onFailure);
};

/**
 * Creates a video thumbnail data url from video file.
 *
 * @param {string} url Video URL.
 * @return {!Promise<Blob>}  Promise that resolves with the data url of video
 *    thumbnail.
 * @private
 */
ImageRequest.prototype.createVideoThumbnailUrl_ = function(url) {
  const video =
      assertInstanceof(document.createElement('video'), HTMLVideoElement);
  return Promise
      .race([
        new Promise((resolve, reject) => {
          video.addEventListener('canplay', resolve);
          video.addEventListener('error', reject);
          video.currentTime = ImageRequest.VIDEO_THUMBNAIL_POSITION;
          video.preload = 'auto';
          video.src = url;
          video.load();
        }),
        new Promise((resolve) => {
          setTimeout(resolve, ImageRequest.MAX_MILLISECONDS_TO_LOAD_VIDEO);
        }).then(() => {
          // If we don't receive 'canplay' event after 3 seconds have passed for
          // some reason (e.g. unseekable video), we give up generating
          // thumbnail.
          video.src =
              '';  // Make sure to stop loading remaining part of the video.
          throw new Error('Seeking video failed.');
        })
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
ImageRequest.prototype.load = function(url, onSuccess, onFailure) {
  this.aborted_ = false;

  // Do not call any callbacks when aborting.
  var onMaybeSuccess = /** @type {function(string, Blob)} */ (
      function(contentType, response) {
        // When content type is not available, try to estimate it from url.
        if (!contentType) {
          contentType =
              ImageRequest.ExtensionContentTypeMap[this.extractExtension_(url)];
        }

        if (!this.aborted_) {
          onSuccess(contentType, response);
        }
      }.bind(this));

  var onMaybeFailure = /** @type {function(number=)} */ (function(opt_code) {
    if (!this.aborted_) {
      onFailure();
    }
  }.bind(this));

  // The query parameter is workaround for crbug.com/379678, which forces the
  // browser to obtain the latest contents of the image.
  var noCacheUrl = url + '?nocache=' + Date.now();
  this.xhr_ = ImageRequest.load_(noCacheUrl, onMaybeSuccess, onMaybeFailure);
};

/**
 * Extracts extension from url.
 * @param {string} url Url.
 * @return {string} Extracted extension, e.g. png.
 */
ImageRequest.prototype.extractExtension_ = function(url) {
  var result = (/\.([a-zA-Z]+)$/i).exec(url);
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
ImageRequest.load_ = function(url, onSuccess, onFailure) {
  let xhr = new XMLHttpRequest();
  xhr.responseType = 'blob';

  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status != 200) {
      onFailure(xhr.status);
      return;
    }
    let response = /** @type {Blob} */ (xhr.response);
    let contentType = xhr.getResponseHeader('Content-Type') || response.type;
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
ImageRequest.prototype.sendImage_ = function(imageChanged) {
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
ImageRequest.prototype.sendImageData_ = function(width, height, data) {
  const result = {width, height, ifd: this.ifd_, data};
  this.sendResponse_(new LoadImageResponse(
      LoadImageResponseStatus.SUCCESS, this.getClientTaskId(), result));
};

/**
 * Handler, when contents are loaded into the image element. Performs resizing
 * and finalizes the request process.
 * @private
 */
ImageRequest.prototype.onImageLoad_ = function() {
  // Perform processing if the url is not a data url, or if there are some
  // operations requested.
  if (!(this.request_.url.match(/^data/) ||
        this.request_.url.match(/^drivefs:/)) ||
      ImageLoaderUtil.shouldProcess(
          this.image_.width, this.image_.height, this.request_)) {
    ImageLoaderUtil.resizeAndCrop(this.image_, this.canvas_, this.request_);
    ImageLoaderUtil.convertColorSpace(
        this.canvas_, this.request_.colorSpace || ColorSpace.SRGB);
    this.sendImage_(true);  // Image changed.
  } else {
    this.sendImage_(false);  // Image not changed.
  }
  this.cleanup_();
  this.downloadCallback_();
};

/**
 * Handler, when loading of the image fails. Sends a failure response and
 * finalizes the request process.
 * @private
 */
ImageRequest.prototype.onImageError_ = function() {
  this.sendResponse_(new LoadImageResponse(
      LoadImageResponseStatus.ERROR, this.getClientTaskId()));
  this.cleanup_();
  this.downloadCallback_();
};

/**
 * Cancels the request.
 */
ImageRequest.prototype.cancel = function() {
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
ImageRequest.prototype.cleanup_ = function() {
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
