// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import {ImageTransformParam} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_orientation.js';
import {LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {FileType} from '../../common/js/file_type.js';

/**
 * Loads a thumbnail using provided url. In CANVAS mode, loaded images
 * are attached as <canvas> element, while in IMAGE mode as <img>.
 * <canvas> renders faster than <img>, however has bigger memory overhead.
 */
export class ThumbnailLoader {
  /**
   * @param {!Entry} entry File entry.
   * @param {!ThumbnailLoader.LoaderType=} opt_loaderType Canvas or Image
   *     loader, default: IMAGE.
   * @param {?Object=} opt_metadata Metadata object.
   * @param {string=} opt_mediaType Media type.
   * @param {!Array<ThumbnailLoader.LoadTarget>=} opt_loadTargets The list of
   *     load targets in preferential order. The default value is
   *     [CONTENT_METADATA, EXTERNAL_METADATA, FILE_ENTRY].
   * @param {number=} opt_priority Priority, the highest is 0. default: 2.
   */
  constructor(
      entry, opt_loaderType, opt_metadata, opt_mediaType, opt_loadTargets,
      opt_priority) {
    /** @private @type {boolean} */
    this.canvasUpToDate_ = false;

    /** @private @type {?Image} */
    this.image_ = null;

    /** @private @type {?number} */
    this.taskId_ = null;

    /** @private @type {?HTMLCanvasElement} */
    this.canvas_ = null;

    const loadTargets = opt_loadTargets || [
      ThumbnailLoader.LoadTarget.CONTENT_METADATA,
      ThumbnailLoader.LoadTarget.EXTERNAL_METADATA,
      ThumbnailLoader.LoadTarget.FILE_ENTRY,
    ];

    /** @private @const @type {!Entry} */
    this.entry_ = entry;

    /** @private @const @type {string} */
    this.mediaType_ = opt_mediaType || FileType.getMediaType(entry);

    /** @private @const @type {!ThumbnailLoader.LoaderType} */
    this.loaderType_ = opt_loaderType || ThumbnailLoader.LoaderType.IMAGE;

    /** @private @const @type {?Object|undefined} */
    this.metadata_ = opt_metadata;

    /** @private @const @type {number} */
    this.priority_ = (opt_priority !== undefined) ? opt_priority : 2;

    /**
     * The image transform from metadata.
     *
     * TODO(tapted): I suspect this actually needs to be more complicated, but
     * it can't be properly type-checked so long as |opt_metadata| is passed in
     * merely as "{Object}".
     *
     * @private @type {?ImageTransformParam}
     */
    this.transform_ = null;

    /** @private @type {?ThumbnailLoader.LoadTarget} */
    this.loadTarget_ = null;

    if (!opt_metadata) {
      this.thumbnailUrl_ = entry.toURL();  // Use the URL directly.
      this.loadTarget_ = ThumbnailLoader.LoadTarget.FILE_ENTRY;
      return;
    }

    this.fallbackUrl_ = null;
    this.thumbnailUrl_ = null;
    // @ts-ignore: error TS2339: Property 'external' does not exist on type
    // 'Object'.
    if (opt_metadata.external && opt_metadata.external.customIconUrl) {
      // @ts-ignore: error TS2339: Property 'external' does not exist on type
      // 'Object'.
      this.fallbackUrl_ = opt_metadata.external.customIconUrl;
    }
    // @ts-ignore: error TS2339: Property 'contentMimeType' does not exist on
    // type 'Object'.
    const mimeType = opt_metadata && opt_metadata.contentMimeType;

    for (let i = 0; i < loadTargets.length; i++) {
      switch (loadTargets[i]) {
        case ThumbnailLoader.LoadTarget.CONTENT_METADATA:
          // @ts-ignore: error TS2339: Property 'thumbnail' does not exist on
          // type 'Object'.
          if (opt_metadata.thumbnail && opt_metadata.thumbnail.url) {
            // @ts-ignore: error TS2339: Property 'thumbnail' does not exist on
            // type 'Object'.
            this.thumbnailUrl_ = opt_metadata.thumbnail.url;
            this.transform_ =
                // @ts-ignore: error TS2339: Property 'thumbnail' does not exist
                // on type 'Object'.
                opt_metadata.thumbnail && opt_metadata.thumbnail.transform;
            this.loadTarget_ = ThumbnailLoader.LoadTarget.CONTENT_METADATA;
          }
          break;
        case ThumbnailLoader.LoadTarget.EXTERNAL_METADATA:
          // @ts-ignore: error TS2339: Property 'external' does not exist on
          // type 'Object'.
          if (opt_metadata.external && opt_metadata.external.thumbnailUrl &&
              // @ts-ignore: error TS2339: Property 'external' does not exist on
              // type 'Object'.
              (!opt_metadata.external.present ||
               !FileType.isImage(entry, mimeType))) {
            // @ts-ignore: error TS2339: Property 'external' does not exist on
            // type 'Object'.
            this.thumbnailUrl_ = opt_metadata.external.thumbnailUrl;
            this.croppedThumbnailUrl_ =
                // @ts-ignore: error TS2339: Property 'external' does not exist
                // on type 'Object'.
                opt_metadata.external.croppedThumbnailUrl;
            this.loadTarget_ = ThumbnailLoader.LoadTarget.EXTERNAL_METADATA;
          }
          break;
        case ThumbnailLoader.LoadTarget.FILE_ENTRY:
          if (FileType.isImage(entry, mimeType) ||
              FileType.isVideo(entry, mimeType) ||
              FileType.isRaw(entry, mimeType) ||
              FileType.isPDF(entry, mimeType)) {
            this.thumbnailUrl_ = entry.toURL();
            this.transform_ =
                // @ts-ignore: error TS2339: Property 'media' does not exist on
                // type 'Object'.
                opt_metadata.media && opt_metadata.media.imageTransform;
            this.loadTarget_ = ThumbnailLoader.LoadTarget.FILE_ENTRY;
          }
          break;
        default:
          assertNotReached('Unkonwn load type: ' + loadTargets[i]);
      }
      if (this.thumbnailUrl_) {
        break;
      }
    }

    if (!this.thumbnailUrl_ && this.fallbackUrl_) {
      // Use fallback as the primary thumbnail.
      this.thumbnailUrl_ = this.fallbackUrl_;
      this.fallbackUrl_ = null;
    }  // else the generic thumbnail based on the media type will be used.
  }

  /**
   * Returns the target of loading.
   * @return {?ThumbnailLoader.LoadTarget}
   */
  getLoadTarget() {
    return this.loadTarget_;
  }

  /**
   * Loads and attaches an image.
   *
   * @param {!Element} box Container element.
   * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
   * @param {function(Image):void} onSuccess Success callback, accepts the
   *     image.
   * @param {number} autoFillThreshold Auto fill threshold.
   * @param {number} boxWidth Container box's width.
   * @param {number} boxHeight Container box's height.
   */
  load(box, fillMode, onSuccess, autoFillThreshold, boxWidth, boxHeight) {
    if (!this.thumbnailUrl_) {
      // Relevant CSS rules are in file_types.css.
      box.setAttribute('generic-thumbnail', this.mediaType_);
      return;
    }

    this.cancel();
    this.canvasUpToDate_ = false;
    // @ts-ignore: error TS2322: Type 'HTMLImageElement' is not assignable to
    // type 'new (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_ = new Image();
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'new (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_.setAttribute('alt', this.entry_.name);
    // @ts-ignore: error TS2339: Property 'onload' does not exist on type 'new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_.onload = () => {
      this.attachImage_(box, fillMode, autoFillThreshold, boxWidth, boxHeight);
      // @ts-ignore: error TS2345: Argument of type 'HTMLImageElement | null' is
      // not assignable to parameter of type 'new (width?: number | undefined,
      // height?: number | undefined) => HTMLImageElement'.
      onSuccess(this.image_);
    };
    // @ts-ignore: error TS2339: Property 'onerror' does not exist on type 'new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_.onerror = () => {
      if (this.fallbackUrl_) {
        this.thumbnailUrl_ = this.fallbackUrl_;
        this.fallbackUrl_ = null;
        this.load(
            box, fillMode, onSuccess,
            ThumbnailLoader.AUTO_FILL_THRESHOLD_DEFAULT_VALUE, box.clientWidth,
            box.clientHeight);
      } else {
        box.setAttribute('generic-thumbnail', this.mediaType_);
      }
    };

    // @ts-ignore: error TS2339: Property 'src' does not exist on type 'new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    if (this.image_.src) {
      console.warn('Thumbnail already loaded: ' + this.thumbnailUrl_);
      return;
    }

    // TODO(mtomasz): Smarter calculation of the requested size.
    // @ts-ignore: error TS6133: 'wasAttached' is declared but its value is
    // never read.
    const wasAttached = box.ownerDocument.contains(box);
    // @ts-ignore: error TS2339: Property 'filesystem' does not exist on type
    // 'Object'.
    const modificationTime = this.metadata_ && this.metadata_.filesystem &&
        // @ts-ignore: error TS2339: Property 'filesystem' does not exist on
        // type 'Object'.
        this.metadata_.filesystem.modificationTime &&
        // @ts-ignore: error TS2339: Property 'filesystem' does not exist on
        // type 'Object'.
        this.metadata_.filesystem.modificationTime.getTime();
    this.taskId_ = ImageLoaderClient.loadToImage(
        LoadImageRequest.createRequest({
          url: this.thumbnailUrl_,
          maxWidth: ThumbnailLoader.THUMBNAIL_MAX_WIDTH,
          maxHeight: ThumbnailLoader.THUMBNAIL_MAX_HEIGHT,
          cache: true,
          priority: this.priority_,
          timestamp: modificationTime,
          orientation: this.transform_,
        }),
        // @ts-ignore: error TS2345: Argument of type '(new (width?: number |
        // undefined, height?: number | undefined) => HTMLImageElement) | null'
        // is not assignable to parameter of type 'HTMLImageElement'.
        this.image_, () => {}, () => {
          // @ts-ignore: error TS2721: Cannot invoke an object which is possibly
          // 'null'.
          this.image_.onerror(new Event('load-error'));
        });
  }

  /**
   * Loads thumbnail as dataUrl. If the thumbnail dataUrl can be fetched from
   * metadata, this fetches it from it. Otherwise, this tries to load it from
   * thumbnail loader.
   * Compared with ThumbnailLoader.load, this method does not provide a
   * functionality to fit image to a box.
   *
   * @param {ThumbnailLoader.FillMode} fillMode Only FIT and OVER_FILL are
   *     supported. This takes effect only when an external thumbnail source
   *     is used.
   * @return {!Promise<{data:string, width:number, height:number}>} A promise
   *     which is resolved when data url is fetched.
   *
   * TODO(yawano): Support cancel operation.
   */
  loadAsDataUrl(fillMode) {
    assert(
        fillMode === ThumbnailLoader.FillMode.FIT ||
        fillMode === ThumbnailLoader.FillMode.OVER_FILL);

    return new Promise((resolve, reject) => {
      let requestUrl = this.thumbnailUrl_;

      if (fillMode === ThumbnailLoader.FillMode.OVER_FILL) {
        // Use the croppedThumbnailUrl_ if available.
        requestUrl = this.croppedThumbnailUrl_ || this.thumbnailUrl_;
      }

      if (!requestUrl) {
        const error = LoadImageResponseStatus.ERROR;
        reject(new LoadImageResponse(error, 0));
        return;
      }

      // @ts-ignore: error TS2339: Property 'filesystem' does not exist on type
      // 'Object'.
      const modificationTime = this.metadata_ && this.metadata_.filesystem &&
          // @ts-ignore: error TS2339: Property 'filesystem' does not exist on
          // type 'Object'.
          this.metadata_.filesystem.modificationTime &&
          // @ts-ignore: error TS2339: Property 'filesystem' does not exist on
          // type 'Object'.
          this.metadata_.filesystem.modificationTime.getTime();

      // Load using ImageLoaderClient.
      const request = LoadImageRequest.createRequest({
        url: requestUrl,
        maxWidth: ThumbnailLoader.THUMBNAIL_MAX_WIDTH,
        maxHeight: ThumbnailLoader.THUMBNAIL_MAX_HEIGHT,
        cache: true,
        priority: this.priority_,
        timestamp: modificationTime,
        orientation: this.transform_,
      });

      if (fillMode === ThumbnailLoader.FillMode.OVER_FILL) {
        // Set crop option to image loader. Since image of croppedThumbnailUrl_
        // is 360x360 with current implementation, it's no problem to crop it.
        request.width = 360;
        request.height = 360;
        request.crop = true;
      }

      ImageLoaderClient.getInstance().load(request, result => {
        if (!result || result.status !== LoadImageResponseStatus.SUCCESS) {
          reject(result);
        } else {
          // @ts-ignore: error TS2345: Argument of type 'LoadImageResponse' is
          // not assignable to parameter of type '{ data: string; width: number;
          // height: number; } | PromiseLike<{ data: string; width: number;
          // height: number; }>'.
          resolve(result);
        }
      });
    });
  }

  /**
   * Cancels loading the current image.
   */
  cancel() {
    if (this.taskId_) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.image_.onload = () => {};
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.image_.onerror = () => {};
      ImageLoaderClient.getInstance().cancel(this.taskId_);
      this.taskId_ = null;
    }
  }

  /**
   * @return {boolean} True if a valid image is loaded.
   */
  hasValidImage() {
    // @ts-ignore: error TS2339: Property 'height' does not exist on type 'new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    return !!(this.image_ && this.image_.width && this.image_.height);
  }

  /**
   * @return {number} Image width.
   */
  getWidth() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.image_.width;
  }

  /**
   * @return {number} Image height.
   */
  getHeight() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.image_.height;
  }

  /**
   * Load an image but do not attach it.
   *
   * @param {function(boolean):void} callback Callback, parameter is true if the
   *     image has loaded successfully or a stock icon has been used.
   */
  loadDetachedImage(callback) {
    if (!this.thumbnailUrl_) {
      callback(true);
      return;
    }

    this.cancel();
    this.canvasUpToDate_ = false;
    // @ts-ignore: error TS2322: Type 'HTMLImageElement' is not assignable to
    // type 'new (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_ = new Image();
    // @ts-ignore: error TS2339: Property 'onload' does not exist on type 'new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_.onload = callback.bind(null, true);
    // @ts-ignore: error TS2339: Property 'onerror' does not exist on type 'new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement'.
    this.image_.onerror = callback.bind(null, false);

    // TODO(mtomasz): Smarter calculation of the requested size.
    // @ts-ignore: error TS2339: Property 'filesystem' does not exist on type
    // 'Object'.
    const modificationTime = this.metadata_ && this.metadata_.filesystem &&
        // @ts-ignore: error TS2339: Property 'filesystem' does not exist on
        // type 'Object'.
        this.metadata_.filesystem.modificationTime &&
        // @ts-ignore: error TS2339: Property 'filesystem' does not exist on
        // type 'Object'.
        this.metadata_.filesystem.modificationTime.getTime();
    this.taskId_ = ImageLoaderClient.loadToImage(
        LoadImageRequest.createRequest({
          url: this.thumbnailUrl_,
          maxWidth: ThumbnailLoader.THUMBNAIL_MAX_WIDTH,
          maxHeight: ThumbnailLoader.THUMBNAIL_MAX_HEIGHT,
          cache: true,
          priority: this.priority_,
          timestamp: modificationTime,
          orientation: this.transform_,
        }),
        // @ts-ignore: error TS2345: Argument of type '(new (width?: number |
        // undefined, height?: number | undefined) => HTMLImageElement) | null'
        // is not assignable to parameter of type 'HTMLImageElement'.
        this.image_, () => {}, () => {
          // @ts-ignore: error TS2721: Cannot invoke an object which is possibly
          // 'null'.
          this.image_.onerror(new Event('load-error'));
        });
  }

  /**
   * Renders the thumbnail into either canvas or an image element.
   * @private
   */
  renderMedia_() {
    if (this.loaderType_ !== ThumbnailLoader.LoaderType.CANVAS) {
      return;
    }

    if (!this.canvas_) {
      this.canvas_ =
          /** @type {HTMLCanvasElement} */ (document.createElement('canvas'));
    }

    // Copy the image to a canvas if the canvas is outdated.
    // At this point, image transformation is not applied because we attach
    // style attribute to an img element in attachImage() instead.
    if (!this.canvasUpToDate_) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.canvas_.width = this.image_.width;
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.canvas_.height = this.image_.height;
      const context = this.canvas_.getContext('2d');
      // @ts-ignore: error TS2345: Argument of type 'HTMLImageElement | null' is
      // not assignable to parameter of type 'CanvasImageSource'.
      context.drawImage(this.image_, 0, 0);
      this.canvasUpToDate_ = true;
    }
  }

  /**
   * Attach the image to a given element.
   * @param {!Element} box Container element.
   * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
   * @param {number} autoFillThreshold Threshold value which is used for fill
   *     mode auto.
   * @param {number} boxWidth Container box's width.
   * @param {number} boxHeight Container box's height.
   * @private
   */
  attachImage_(box, fillMode, autoFillThreshold, boxWidth, boxHeight) {
    if (!this.hasValidImage()) {
      box.setAttribute('generic-thumbnail', this.mediaType_);
      return;
    }

    this.renderMedia_();
    const attachableMedia =
        this.loaderType_ === ThumbnailLoader.LoaderType.CANVAS ? this.canvas_ :
                                                                 this.image_;

    ThumbnailLoader.centerImage_(
        // @ts-ignore: error TS2345: Argument of type 'HTMLImageElement |
        // HTMLCanvasElement | null' is not assignable to parameter of type
        // '(new (width?: number | undefined, height?: number | undefined) =>
        // HTMLImageElement) | HTMLCanvasElement'.
        box, attachableMedia, fillMode, autoFillThreshold, boxWidth, boxHeight);

    // @ts-ignore: error TS18047: 'attachableMedia' is possibly 'null'.
    if (attachableMedia.parentNode !== box) {
      box.textContent = '';
      // @ts-ignore: error TS2345: Argument of type 'HTMLImageElement |
      // HTMLCanvasElement | null' is not assignable to parameter of type
      // 'Node'.
      box.appendChild(attachableMedia);
    }

    if (!this.taskId_) {
      // @ts-ignore: error TS18047: 'attachableMedia' is possibly 'null'.
      attachableMedia.classList.add('cached');
    }
  }

  /**
   * Gets the loaded image.
   *
   * @return {Image|HTMLCanvasElement} Either image or a canvas object.
   */
  getImage() {
    this.renderMedia_();
    // @ts-ignore: error TS2322: Type 'HTMLImageElement | HTMLCanvasElement |
    // null' is not assignable to type '(new (width?: number | undefined,
    // height?: number | undefined) => HTMLImageElement) | HTMLCanvasElement'.
    return (this.loaderType_ === ThumbnailLoader.LoaderType.IMAGE) ?
        this.image_ :
        this.canvas_;
  }

  /**
   * Updates the image style to fit/fill the container.
   *
   * Using webkit center packing does not align the image properly, so we need
   * to wait until the image loads and its dimensions are known, then manually
   * position it at the center.
   *
   * @param {Element} box Containing element.
   * @param {Image|HTMLCanvasElement} img Element containing an image.
   * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
   * @param {number} autoFillThreshold Threshold value which is used for fill
   *     mode auto.
   * @param {number} boxWidth Container box's width.
   * @param {number} boxHeight Container box's height.
   * @private
   */
  static centerImage_(
      // @ts-ignore: error TS6133: 'box' is declared but its value is never
      // read.
      box, img, fillMode, autoFillThreshold, boxWidth, boxHeight) {
    // @ts-ignore: error TS2339: Property 'width' does not exist on type '(new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement) | HTMLCanvasElement'.
    const imageWidth = img.width;
    // @ts-ignore: error TS2339: Property 'height' does not exist on type '(new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement) | HTMLCanvasElement'.
    const imageHeight = img.height;

    let fractionX;
    let fractionY;

    let fill;
    switch (fillMode) {
      case ThumbnailLoader.FillMode.FILL:
      case ThumbnailLoader.FillMode.OVER_FILL:
        fill = true;
        break;
      case ThumbnailLoader.FillMode.FIT:
        fill = false;
        break;
      case ThumbnailLoader.FillMode.AUTO:
        const imageRatio = imageWidth / imageHeight;
        let boxRatio = 1.0;
        if (boxWidth && boxHeight) {
          boxRatio = boxWidth / boxHeight;
        }
        // Cropped area in percents.
        const ratioFactor = boxRatio / imageRatio;
        fill = (ratioFactor >= 1.0 - autoFillThreshold) &&
            (ratioFactor <= 1.0 + autoFillThreshold);
        break;
    }

    if (boxWidth && boxHeight) {
      // When we know the box size we can position the image correctly even
      // in a non-square box.
      const fitScaleX = boxWidth / imageWidth;
      const fitScaleY = boxHeight / imageHeight;

      let scale = fill ? Math.max(fitScaleX, fitScaleY) :
                         Math.min(fitScaleX, fitScaleY);

      if (fillMode !== ThumbnailLoader.FillMode.OVER_FILL) {
        scale = Math.min(scale, 1);  // Never overscale.
      }

      fractionX = imageWidth * scale / boxWidth;
      fractionY = imageHeight * scale / boxHeight;
    } else {
      // We do not know the box size so we assume it is square.
      // Compute the image position based only on the image dimensions.
      // First try vertical fit or horizontal fill.
      fractionX = imageWidth / imageHeight;
      fractionY = 1;
      if ((fractionX < 1) === !!fill) {  // Vertical fill or horizontal fit.
        fractionY = 1 / fractionX;
        fractionX = 1;
      }
    }

    // @ts-ignore: error TS7006: Parameter 'fraction' implicitly has an 'any'
    // type.
    function percent(fraction) {
      return (fraction * 100).toFixed(2) + '%';
    }

    // @ts-ignore: error TS2339: Property 'style' does not exist on type '(new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement) | HTMLCanvasElement'.
    img.style.width = percent(fractionX);
    // @ts-ignore: error TS2339: Property 'style' does not exist on type '(new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement) | HTMLCanvasElement'.
    img.style.height = percent(fractionY);
    // @ts-ignore: error TS2339: Property 'style' does not exist on type '(new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement) | HTMLCanvasElement'.
    img.style.left = percent((1 - fractionX) / 2);
    // @ts-ignore: error TS2339: Property 'style' does not exist on type '(new
    // (width?: number | undefined, height?: number | undefined) =>
    // HTMLImageElement) | HTMLCanvasElement'.
    img.style.top = percent((1 - fractionY) / 2);
  }
}

/**
 * In percents (0.0 - 1.0), how much area can be cropped to fill an image
 * in a container, when loading a thumbnail in FillMode.AUTO mode.
 * The default 30% value allows to fill 16:9, 3:2 pictures in 4:3 element.
 * @const @type {number}
 */
ThumbnailLoader.AUTO_FILL_THRESHOLD_DEFAULT_VALUE = 0.3;

/**
 * Type of displaying a thumbnail within a box.
 * @enum {number}
 */
ThumbnailLoader.FillMode = {
  FILL: 0,       // Fill whole box. Image may be cropped.
  FIT: 1,        // Keep aspect ratio, do not crop.
  OVER_FILL: 2,  // Fill whole box with possible stretching.
  AUTO: 3,       // Try to fill, but if incompatible aspect ratio, then fit.
};

/**
 * Type of element to store the image.
 * @enum {number}
 */
ThumbnailLoader.LoaderType = {
  IMAGE: 0,
  CANVAS: 1,
};

/**
 * Load target of ThumbnailLoader.
 * @enum {string}
 */
ThumbnailLoader.LoadTarget = {
  // e.g. Drive thumbnail, FSP thumbnail.
  EXTERNAL_METADATA: 'externalMetadata',
  // e.g. EXIF thumbnail.
  CONTENT_METADATA: 'contentMetadata',
  // Image file itself.
  FILE_ENTRY: 'fileEntry',
};

/**
 * Maximum thumbnail's width when generating from the full resolution image.
 * @const
 * @type {number}
 */
ThumbnailLoader.THUMBNAIL_MAX_WIDTH = 500;

/**
 * Maximum thumbnail's height when generating from the full resolution image.
 * @const
 * @type {number}
 */
ThumbnailLoader.THUMBNAIL_MAX_HEIGHT = 500;
