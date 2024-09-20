// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFileTypeForName} from 'chrome://file-manager/common/js/file_types_base.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {ImageCache} from './cache.js';
import {resizeAndCrop, shouldProcess} from './image_loader_util.js';
import {ImageOrientation} from './image_orientation.js';
import {cacheKey, type LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from './load_image_request.js';
import {PiexLoader} from './piex_loader.js';
import type {PrivateApi} from './sw_od_messages.js';

const ExtensionContentTypeMap = new Map<string, string>([
  ['gif', 'image/gif'],
  ['png', 'image/png'],
  ['svg', 'image/svg'],
  ['bmp', 'image/bmp'],
  ['jpg', 'image/jpeg'],
  ['jpeg', 'image/jpeg'],
]);

const adpRegExp = RegExp(
    '^filesystem:chrome-extension://[a-z]+/external/arc-documents-provider/');

/**
 * Calls the imageLoaderPrivate API with the given message.
 *
 * @param msg The imageLoaderPrivate call arguments.
 * @return A promise for the thumbnailDataUrl.
 */
function callImageLoaderPrivate(msg: PrivateApi): Promise<string> {
  return new Promise<string>((resolve, reject) => {
    const callback = (thumbnailDataUrl: string) => {
      if (chrome.runtime.lastError) {
        console.warn(chrome.runtime.lastError.message);
        reject(chrome.runtime.lastError);
      } else if (thumbnailDataUrl) {
        resolve(thumbnailDataUrl);
      } else {
        reject();
      }
    };

    if (msg.apiMethod === 'getDriveThumbnail') {
      chrome.imageLoaderPrivate.getDriveThumbnail(
          msg.params.url,
          msg.params.cropToSquare,
          callback,
      );
    } else if (msg.apiMethod === 'getPdfThumbnail') {
      chrome.imageLoaderPrivate.getPdfThumbnail(
          msg.params.url,
          msg.params.width,
          msg.params.height,
          callback,
      );
    } else if (msg.apiMethod === 'getArcDocumentsProviderThumbnail') {
      chrome.imageLoaderPrivate.getArcDocumentsProviderThumbnail(
          msg.params.url,
          msg.params.widthHint,
          msg.params.heightHint,
          callback,
      );
    }
  });
}

/**
 * Creates and starts downloading and then resizing of the image. Finally,
 * returns the image using the callback.
 */
export class ImageRequestTask {
  /**
   * The maximum milliseconds to load video. If loading video exceeds the limit,
   * we give up generating video thumbnail and free the consumed memory.
   */
  static readonly MAX_MILLISECONDS_TO_LOAD_VIDEO: number = 10000;

  /**
   * The default width of a non-square thumbnail. The value is set to match the
   * behavior of drivefs thumbnail generation.
   * See chromeos/ash/components/drivefs/mojom/drivefs.mojom
   */
  static readonly DEFAULT_THUMBNAIL_SQUARE_SIZE: number = 360;

  /**
   * The default width of a non-square thumbnail. The value is set to match the
   * behavior of drivefs thumbnail generation.
   * See chromeos/ash/components/drivefs/mojom/drivefs.mojom
   */
  static readonly DEFAULT_THUMBNAIL_WIDTH: number = 500;

  /**
   * The default height of a non-square thumbnail. The value is set to match the
   * behavior of drivefs thumbnail generation.
   * See chromeos/ash/components/drivefs/mojom/drivefs.mojom
   */
  static readonly DEFAULT_THUMBNAIL_HEIGHT: number = 500;

  /**
   * Temporary image used to download images.
   */
  private image_: HTMLImageElement = new Image();

  /**
   * MIME type of the fetched image.
   */
  private contentType_?: string;

  /**
   * IFD data of the fetched image. Only RAW images provide a non-null
   * ifd at this time. Drive images might provide an ifd in future.
   */
  private ifd_?: string;

  /**
   * Used to download remote images using http:// or https:// protocols.
   */
  private xhr_: XMLHttpRequest|null = null;

  /**
   * Temporary canvas used to resize and compress the image.
   */
  private canvas_: HTMLCanvasElement;

  private renderOrientation_: ImageOrientation|null = null;

  /**
   * Callback to be called once downloading is finished.
   */
  private downloadCallback_: null|VoidCallback = null;

  private aborted_: boolean = false;

  /**
   * @param id Request ID.
   * @param cache Cache object.
   * @param request Request message as a hash array.
   * @param callback Response handler.
   */
  constructor(
      private id_: string,
      private cache_: ImageCache,
      private request_: LoadImageRequest,
      private sendResponse_: (a: LoadImageResponse) => void,
  ) {
    this.canvas_ = document.createElement('canvas');
  }

  /**
   * Extracts MIME type of a data URL.
   * @param dataUrl Data URL.
   * @return MIME type string, or null if the URL is invalid.
   */
  static getDataUrlMimeType(dataUrl?: string): string|undefined {
    const dataUrlMatches = (dataUrl || '').match(/^data:([^,;]*)[,;]/);
    return dataUrlMatches ? dataUrlMatches[1] : undefined;
  }

  /**
   * Returns ID of the request.
   * @return Request ID.
   */
  getId(): string {
    return this.id_;
  }

  getClientTaskId(): number {
    // Every incoming request should have been given a taskId.
    assert(this.request_.taskId);
    return this.request_.taskId;
  }

  /**
   * Returns priority of the request. The higher priority, the faster it will
   * be handled. The highest priority is 0. The default one is 2.
   *
   * @return Priority.
   */
  getPriority(): number {
    return this.request_.priority !== undefined ? this.request_.priority : 2;
  }

  /**
   * Tries to load the image from cache, if it exists in the cache, and sends
   * the response. Fails if the image is not found in the cache.
   *
   * @param onSuccess Success callback.
   * @param onFailure Failure callback.
   */
  loadFromCacheAndProcess(onSuccess: VoidCallback, onFailure: VoidCallback) {
    this.loadFromCache_(
        (width: number, height: number, ifd?: string, data?: string) => {
          // Found in cache.
          this.ifd_ = ifd;
          this.sendImageData_(width, height, data!);
          onSuccess();
        },
        onFailure,
    );  // Not found in cache.
  }

  /**
   * Tries to download the image, resizes and sends the response.
   *
   * @param callback Completion callback.
   */
  downloadAndProcess(callback: VoidCallback) {
    if (this.downloadCallback_) {
      throw new Error('Downloading already started.');
    }

    this.downloadCallback_ = callback;
    this.downloadThumbnail_(
        this.onImageLoad_.bind(this),
        this.onImageError_.bind(this),
    );
  }

  /**
   * Fetches the image from the persistent cache.
   *
   * @param onSuccess callback with the image width, height, ?ifd, and data.
   * @param onFailure Failure callback.
   */
  private loadFromCache_(
      onSuccess: (
          width: number,
          height: number,
          ifd?: string,
          data?: string,
          ) => void,
      onFailure: VoidCallback,
  ) {
    const key = cacheKey(this.request_);

    if (!key) {
      // Cache key is not provided for the request.
      onFailure();
      return;
    }

    if (!this.request_.cache) {
      // Cache is disabled for this request; therefore, remove it from cache
      // if existed.
      this.cache_.removeImage(key);
      onFailure();
      return;
    }

    const timestamp = this.request_.timestamp;
    if (!timestamp) {
      // Persistent cache is available only when a timestamp is provided.
      onFailure();
      return;
    }

    this.cache_.loadImage(key, timestamp, onSuccess, onFailure);
  }

  /**
   * Saves the image to the persistent cache.
   *
   * @param width Image width.
   * @param height Image height.
   * @param data Image data.
   */
  private saveToCache_(width: number, height: number, data: string) {
    const timestamp = this.request_.timestamp;

    if (!this.request_.cache || !timestamp) {
      // Persistent cache is available only when a timestamp is provided.
      return;
    }

    const key = cacheKey(this.request_);
    if (!key) {
      // Cache key is not provided for the request.
      return;
    }

    this.cache_.saveImage(key, timestamp, width, height, this.ifd_, data);
  }

  /**
   * Gets the target image size for external thumbnails, where supported.
   * The defaults replicate drivefs thumbnailer behavior.
   */
  private targetThumbnailSize_(): {width: number, height: number} {
    const crop = !!this.request_.crop;
    const defaultWidth = crop ? ImageRequestTask.DEFAULT_THUMBNAIL_SQUARE_SIZE :
                                ImageRequestTask.DEFAULT_THUMBNAIL_WIDTH;
    const defaultHeight = crop ?
        ImageRequestTask.DEFAULT_THUMBNAIL_SQUARE_SIZE :
        ImageRequestTask.DEFAULT_THUMBNAIL_HEIGHT;
    return {
      width: this.request_.width || defaultWidth,
      height: this.request_.height || defaultHeight,
    };
  }

  /**
   * Loads |this.image_| with the |this.request_.url| source or the thumbnail
   * image of the source.
   *
   * @param onSuccess Success callback.
   * @param onFailure Failure callback.
   */
  private async downloadThumbnail_(
      onSuccess: VoidCallback,
      onFailure: VoidCallback,
  ) {
    // Load methods below set |this.image_.src|. Call revokeObjectURL(src) to
    // release resources if the image src was created with createObjectURL().
    this.image_.onload = () => {
      URL.revokeObjectURL(this.image_.src);
      onSuccess();
    };
    this.image_.onerror = () => {
      URL.revokeObjectURL(this.image_.src);
      onFailure();
    };

    // Load dataURL sources directly.
    const dataUrlMimeType = ImageRequestTask.getDataUrlMimeType(
        this.request_.url,
    );
    const requestUrl = this.request_.url ?? '';
    if (dataUrlMimeType) {
      this.image_.src = requestUrl;
      this.contentType_ = dataUrlMimeType;
      return;
    }

    const onExternalThumbnail = (dataUrl: string) => {
      this.image_.src = dataUrl;
      this.contentType_ = ImageRequestTask.getDataUrlMimeType(dataUrl);
    };

    // Load Drive source thumbnail.
    const drivefsUrlMatches = requestUrl.match(/^drivefs:(.*)/);
    if (drivefsUrlMatches) {
      callImageLoaderPrivate({
        apiMethod: 'getDriveThumbnail',
        params: {
          url: drivefsUrlMatches[1] || '',
          cropToSquare: !!this.request_.crop,
        },
      })
          .then(onExternalThumbnail)
          .catch(onFailure);
      return;
    }

    // Load PDF source thumbnail.
    if (requestUrl.endsWith('.pdf')) {
      const {width, height} = this.targetThumbnailSize_();
      callImageLoaderPrivate({
        apiMethod: 'getPdfThumbnail',
        params: {
          url: requestUrl,
          width,
          height,
        },
      })
          .then(onExternalThumbnail)
          .catch(onFailure);
      return;
    }

    // Load ARC DocumentsProvider thumbnail, if supported.
    if (requestUrl.match(adpRegExp)) {
      const {width, height} = this.targetThumbnailSize_();
      callImageLoaderPrivate({
        apiMethod: 'getArcDocumentsProviderThumbnail',
        params: {
          url: requestUrl,
          widthHint: width,
          heightHint: height,
        },
      })
          .then(onExternalThumbnail)
          .catch(onFailure);
      return;
    }

    const fileType = getFileTypeForName(requestUrl);

    // Load video source thumbnail.
    if (fileType.type === 'video') {
      this.createVideoThumbnailUrl_(requestUrl)
          .then((url) => {
            this.image_.src = url;
          })
          .catch((error) => {
            console.warn('Video thumbnail error: ', error);
            onFailure();
          });
      return;
    }

    // Load the source directly.
    this.load(
        requestUrl,
        (contentType, blob) => {
          // Load RAW image source thumbnail.
          if (fileType.type === 'raw') {
            blob.arrayBuffer()
                .then(
                    (buffer) => PiexLoader.load(buffer, chrome.runtime.reload))
                .then((data) => {
                  this.renderOrientation_ =
                      ImageOrientation.fromExifOrientation(
                          data.orientation,
                      );
                  this.ifd_ = data.ifd ?? undefined;
                  this.contentType_ = data.mimeType;
                  const blob =
                      new Blob([data.thumbnail], {type: data.mimeType});
                  this.image_.src = URL.createObjectURL(blob);
                })
                .catch(onFailure);
            return;
          }

          this.image_.src = blob ? URL.createObjectURL(blob) : '!';
          this.contentType_ = contentType || undefined;
          if (this.contentType_ === 'image/jpeg') {
            this.renderOrientation_ = ImageOrientation.fromExifOrientation(1);
          }
        },
        onFailure,
    );
  }

  /**
   * Creates a video thumbnail data url from video file.
   *
   * @param url Video URL.
   * @return Promise that resolves with the data url of video
   *    thumbnail.
   */
  private createVideoThumbnailUrl_(url: string): Promise<string> {
    const video: HTMLVideoElement = document.createElement('video');
    return Promise
        .race([
          new Promise<void>((resolve, reject) => {
            video.addEventListener('loadedmetadata', () => {
              video.addEventListener('seeked', () => {
                if (video.readyState >= video.HAVE_CURRENT_DATA) {
                  resolve();
                } else {
                  video.addEventListener('loadeddata', () => resolve());
                }
              });
              // For videos with longer duration (>= 6 seconds), consider the
              // frame at 3rd second, or use the frame at midpoint otherwise.
              // This ensures the target position is always close to the
              // beginning of the video. Seek operations may be costly if the
              // video doesn't contain keyframes for referencing.
              const thumbnailPosition = Math.min(video.duration / 2, 3);
              video.currentTime = thumbnailPosition;
            });
            video.addEventListener('error', reject);
            video.preload = 'metadata';
            video.src = url;
            video.load();
          }),
          new Promise((resolve) => {
            setTimeout(
                resolve, ImageRequestTask.MAX_MILLISECONDS_TO_LOAD_VIDEO);
          }).then(() => {
            // If we can't get the frame at the midpoint of the video after 3
            // seconds have passed for some reason (e.g. unseekable video), we
            // give up generating thumbnail.
            // Make sure to stop loading remaining part of the video.
            video.src = '';
            throw new Error('Seeking video failed.');
          }),
        ])
        .then(() => {
          const canvas: HTMLCanvasElement = document.createElement('canvas');
          canvas.width = video.videoWidth;
          canvas.height = video.videoHeight;
          canvas.getContext('2d')!.drawImage(video, 0, 0);
          // Clearing the `src` helps the decoder to dispose its memory earlier.
          video.src = '';
          return canvas.toDataURL();
        });
  }

  /**
   * Loads an image.
   *
   * @param url URL to the resource to be fetched.
   * @param onSuccess Success callback with the content type and the fetched
   *     data.
   * @param onFailure Failure callback.
   */
  load(
      url: string,
      onSuccess: (contentType: string|undefined, reponse: Blob) => void,
      onFailure: VoidCallback,
  ) {
    this.aborted_ = false;

    // Do not call any callbacks when aborting.
    const onMaybeSuccess = (
        contentType: string|undefined,
        response: Blob,
        ) => {
      // When content type is not available, try to estimate it from url.
      if (!contentType) {
        contentType = ExtensionContentTypeMap.get(this.extractExtension_(url));
      }

      if (!this.aborted_) {
        onSuccess(contentType, response);
      }
    };

    const onMaybeFailure = () => {
      if (!this.aborted_) {
        onFailure();
      }
    };

    // The query parameter is workaround for crbug.com/379678, which forces the
    // browser to obtain the latest contents of the image.
    const noCacheUrl = url + '?nocache=' + Date.now();
    this.xhr_ = ImageRequestTask.load_(
        noCacheUrl,
        onMaybeSuccess,
        onMaybeFailure,
    );
  }

  /**
   * Extracts extension from url.
   * @param url Url.
   * @return Extracted extension, e.g. png.
   */
  private extractExtension_(url: string): string {
    const result = /\.([a-zA-Z]+)$/i.exec(url);
    return result ? result[1] ?? '' : '';
  }

  /**
   * Fetches data using XmlHttpRequest.
   *
   * @param url URL to the resource to be fetched.
   * @param onSuccess Success callback with the content type and the fetched
   *     data.
   * @param onFailure Failure callback with the error code if available.
   * @return XHR instance.
   */
  private static load_(
      url: string,
      onSuccess: (a: string, b: Blob) => void,
      onFailure: (a?: number) => void,
      ): XMLHttpRequest {
    const xhr = new XMLHttpRequest();
    xhr.responseType = 'blob';

    xhr.onreadystatechange = () => {
      if (xhr.readyState !== 4) {
        return;
      }
      if (xhr.status !== 200) {
        onFailure(xhr.status);
        return;
      }
      const response: Blob = xhr.response;
      const contentType =
          xhr.getResponseHeader('Content-Type') || response.type;
      onSuccess(contentType, response);
    };

    // Perform a xhr request.
    try {
      xhr.open('GET', url, true);
      xhr.send();
    } catch (e) {
      onFailure();
    }

    return xhr;
  }

  /**
   * Sends the resized image via the callback. If the image has been changed,
   * then packs the canvas contents, otherwise sends the raw image data.
   *
   * @param imageChanged Whether the image has been changed.
   */
  private sendImage_(imageChanged: boolean) {
    let width: number;
    let height: number;
    let data: string;

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
        case 'image/jpeg':
          data = this.canvas_.toDataURL('image/jpeg', 0.9);
          break;
        default:
          data = this.canvas_.toDataURL('image/png');
          break;
      }
    }

    // Send the image data and also save it in the persistent cache.
    this.sendImageData_(width, height, data);
    this.saveToCache_(width, height, data);
  }

  /**
   * Sends the resized image via the callback.
   *
   * @param width Image width.
   * @param height Image height.
   * @param data Image data.
   */
  private sendImageData_(width: number, height: number, data: string) {
    const result = {width, height, ifd: this.ifd_, data};
    this.sendResponse_(
        new LoadImageResponse(
            LoadImageResponseStatus.SUCCESS,
            this.getClientTaskId(),
            result,
            ),
    );
  }

  /**
   * Handler, when contents are loaded into the image element. Performs image
   * processing operations if needed, and finalizes the request process.
   */
  private onImageLoad_() {
    const requestOrientation = this.request_.orientation;

    // Override the request orientation before processing if needed.
    if (this.renderOrientation_) {
      this.request_.orientation = this.renderOrientation_;
    }

    // Perform processing if the url is not a data url, or if there are some
    // operations requested.
    let imageChanged = false;
    const reqUrl = this.request_.url ?? '';
    if (!(reqUrl.match(/^data/) || reqUrl.match(/^drivefs:/)) ||
        shouldProcess(this.image_.width, this.image_.height, this.request_)) {
      resizeAndCrop(this.image_, this.canvas_, this.request_);
      imageChanged = true;  // The image is now on the <canvas>.
    }

    // Restore the request orientation after processing.
    if (this.renderOrientation_) {
      this.request_.orientation = requestOrientation;
    }

    // Finalize the request.
    this.sendImage_(imageChanged);
    this.cleanup_();
    if (this.downloadCallback_) {
      this.downloadCallback_();
    }
  }

  /**
   * Handler, when loading of the image fails. Sends a failure response and
   * finalizes the request process.
   */
  private onImageError_() {
    this.sendResponse_(
        new LoadImageResponse(
            LoadImageResponseStatus.ERROR,
            this.getClientTaskId(),
            ),
    );
    this.cleanup_();
    if (this.downloadCallback_) {
      this.downloadCallback_();
    }
  }

  /**
   * Cancels the request.
   */
  cancel() {
    this.cleanup_();

    // If downloading has started, then call the callback.
    if (this.downloadCallback_) {
      this.downloadCallback_();
    }
  }

  /**
   * Cleans up memory used by this request.
   */
  private cleanup_() {
    this.image_.onerror = () => {};
    this.image_.onload = () => {};

    // Transparent 1x1 pixel gif, to force garbage collecting.
    this.image_.src =
        'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAA' +
        'ABAAEAAAICTAEAOw==';

    this.aborted_ = true;
    if (this.xhr_) {
      this.xhr_.abort();
    }

    // Dispose memory allocated by Canvas.
    this.canvas_.width = 0;
    this.canvas_.height = 0;
  }
}
