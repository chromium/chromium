// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import type {ImageTransformParam} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_orientation.js';
import {createRequest, LoadImageResponse, LoadImageResponseStatus} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';

import {getMediaType, isImage, isPDF, isRaw, isVideo} from '../../common/js/file_type.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';

import type {ThumbnailMetadataItem} from './metadata/thumbnail_model.js';

/**
 * Loads a thumbnail as an <img> using provided url.
 */
export class ThumbnailLoader {
  private image_: HTMLImageElement|null = null;
  private taskId_: number|null = null;
  private mediaType_: string;
  private priority_: number;
  /**
   * The image transform from metadata.
   */
  private transform_?: ImageTransformParam = undefined;
  private loadTarget_: LoadTarget|null = null;
  private thumbnailUrl_: string|null;
  private fallbackUrl_: string|null = null;
  private croppedThumbnailUrl_?: string|null = null;

  /**
   * @param entry_ File entry.
   * @param metadata_ Metadata object.
   * @param mediaType Media type.
   * @param loadTargets The list of load targets in preferential order. The
   *     default value is [CONTENT_METADATA, EXTERNAL_METADATA, FILE_ENTRY].
   * @param priority Priority, the highest is 0. default: 2.
   */
  constructor(
      private entry_: Entry|FilesAppEntry,
      private metadata_?: ThumbnailMetadataItem, mediaType?: string,
      suppliedLoadTargets?: LoadTarget[], priority?: number) {
    const loadTargets = suppliedLoadTargets || [
      LoadTarget.CONTENT_METADATA,
      LoadTarget.EXTERNAL_METADATA,
      LoadTarget.FILE_ENTRY,
    ];

    this.mediaType_ = mediaType || getMediaType(this.entry_);

    this.priority_ = (priority !== undefined) ? priority : 2;

    if (!this.metadata_) {
      this.thumbnailUrl_ = this.entry_.toURL();  // Use the URL directly.
      this.loadTarget_ = LoadTarget.FILE_ENTRY;
      return;
    }

    this.fallbackUrl_ = null;
    this.thumbnailUrl_ = null;
    if (this.metadata_.external && this.metadata_.external.customIconUrl) {
      this.fallbackUrl_ = this.metadata_.external.customIconUrl;
    }
    const mimeType = this.metadata_ && this.metadata_.contentMimeType;

    for (let i = 0; i < loadTargets.length; i++) {
      switch (loadTargets[i]) {
        case LoadTarget.CONTENT_METADATA:
          if (this.metadata_.thumbnail && this.metadata_.thumbnail.url) {
            this.thumbnailUrl_ = this.metadata_.thumbnail.url;
            this.transform_ = (this.metadata_.thumbnail &&
                               this.metadata_.thumbnail.transform) ??
                undefined;
            this.loadTarget_ = LoadTarget.CONTENT_METADATA;
          }
          break;
        case LoadTarget.EXTERNAL_METADATA:
          if (this.metadata_.external && this.metadata_.external.thumbnailUrl &&
              (!this.metadata_.external.present ||
               !isImage(this.entry_, mimeType))) {
            this.thumbnailUrl_ = this.metadata_.external.thumbnailUrl;
            this.croppedThumbnailUrl_ =
                this.metadata_.external.croppedThumbnailUrl;
            this.loadTarget_ = LoadTarget.EXTERNAL_METADATA;
          }
          break;
        case LoadTarget.FILE_ENTRY:
          if (isImage(this.entry_, mimeType) ||
              isVideo(this.entry_, mimeType) || isRaw(this.entry_, mimeType) ||
              isPDF(this.entry_, mimeType)) {
            this.thumbnailUrl_ = this.entry_.toURL();
            this.transform_ =
                (this.metadata_.media && this.metadata_.media.imageTransform) ??
                undefined;
            this.loadTarget_ = LoadTarget.FILE_ENTRY;
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
   */
  getLoadTarget(): LoadTarget|null {
    return this.loadTarget_;
  }

  /**
   * Loads and attaches an image.
   *
   * @param box Container element.
   * @param fillMode Fill mode.
   * @param onSuccess Success callback, accepts the image.
   * @param autoFillThreshold Auto fill threshold.
   * @param boxWidth Container box's width.
   * @param boxHeight Container box's height.
   */
  load(
      box: Element, fillMode: FillMode,
      onSuccess: (image: HTMLImageElement) => void, autoFillThreshold: number,
      boxWidth: number, boxHeight: number) {
    if (!this.thumbnailUrl_) {
      // Relevant CSS rules are in file_types.css.
      box.setAttribute('generic-thumbnail', this.mediaType_);
      return;
    }

    this.cancel();
    this.image_ = new Image();
    this.image_.setAttribute('alt', this.entry_.name);
    this.image_.onload = () => {
      this.attachImage_(box, fillMode, autoFillThreshold, boxWidth, boxHeight);
      assert(this.image_);
      onSuccess(this.image_);
    };
    this.image_.onerror = () => {
      if (this.fallbackUrl_) {
        this.thumbnailUrl_ = this.fallbackUrl_;
        this.fallbackUrl_ = null;
        this.load(
            box, fillMode, onSuccess, AUTO_FILL_THRESHOLD_DEFAULT_VALUE,
            box.clientWidth, box.clientHeight);
      } else {
        box.setAttribute('generic-thumbnail', this.mediaType_);
      }
    };

    if (this.image_.src) {
      console.warn('Thumbnail already loaded: ' + this.thumbnailUrl_);
      return;
    }

    // TODO(mtomasz): Smarter calculation of the requested size.
    const modificationTime = this.metadata_ && this.metadata_.filesystem &&
        this.metadata_.filesystem.modificationTime &&
        this.metadata_.filesystem.modificationTime.getTime();
    this.taskId_ = ImageLoaderClient.loadToImage(
        createRequest({
          url: this.thumbnailUrl_,
          maxWidth: THUMBNAIL_MAX_WIDTH,
          maxHeight: THUMBNAIL_MAX_HEIGHT,
          cache: true,
          priority: this.priority_,
          timestamp: modificationTime,
          orientation: this.transform_,
        }),
        this.image_, () => {}, () => {
          this.image_?.onerror?.(new Event('load-error'));
        });
  }

  /**
   * Loads thumbnail as dataUrl. If the thumbnail dataUrl can be fetched from
   * metadata, this fetches it from it. Otherwise, this tries to load it from
   * thumbnail loader.
   * Compared with ThumbnailLoader.load, this method does not provide a
   * functionality to fit image to a box.
   *
   * @param fillMode Only FIT and OVER_FILL are supported. This takes effect
   *     only when an external thumbnail source is used.
   * @return A promise which is resolved when data url is fetched.
   *
   * TODO(yawano): Support cancel operation.
   */
  loadAsDataUrl(fillMode: FillMode): Promise<LoadImageResponse> {
    assert(fillMode === FillMode.FIT || fillMode === FillMode.OVER_FILL);

    return new Promise((resolve, reject) => {
      let requestUrl = this.thumbnailUrl_;

      if (fillMode === FillMode.OVER_FILL) {
        // Use the croppedThumbnailUrl_ if available.
        requestUrl = this.croppedThumbnailUrl_ || this.thumbnailUrl_;
      }

      if (!requestUrl) {
        const error = LoadImageResponseStatus.ERROR;
        reject(new LoadImageResponse(error, 0));
        return;
      }

      const modificationTime = this.metadata_ && this.metadata_.filesystem &&
          this.metadata_.filesystem.modificationTime &&
          this.metadata_.filesystem.modificationTime.getTime();

      // Load using ImageLoaderClient.
      const request = createRequest({
        url: requestUrl,
        maxWidth: THUMBNAIL_MAX_WIDTH,
        maxHeight: THUMBNAIL_MAX_HEIGHT,
        cache: true,
        priority: this.priority_,
        timestamp: modificationTime,
        orientation: this.transform_,
      });

      if (fillMode === FillMode.OVER_FILL) {
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
      assert(this.image_);
      this.image_.onload = () => {};
      this.image_.onerror = () => {};
      ImageLoaderClient.getInstance().cancel(this.taskId_);
      this.taskId_ = null;
    }
  }

  /**
   * @return True if a valid image is loaded.
   */
  hasValidImage(): boolean {
    return !!(this.image_ && this.image_.width && this.image_.height);
  }

  /**
   * @return Image width.
   */
  getWidth(): number {
    assert(this.image_);
    return this.image_.width;
  }

  /**
   * @return Image height.
   */
  getHeight(): number {
    assert(this.image_);
    return this.image_.height;
  }

  /**
   * Attach the image to a given element.
   * @param box Container element.
   * @param fillMode Fill mode.
   * @param autoFillThreshold Threshold value which is used for fill mode auto.
   * @param boxWidth Container box's width.
   * @param boxHeight Container box's height.
   */
  private attachImage_(
      box: Element, fillMode: FillMode, autoFillThreshold: number,
      boxWidth: number, boxHeight: number) {
    if (!this.hasValidImage()) {
      box.setAttribute('generic-thumbnail', this.mediaType_);
      return;
    }

    assert(this.image_);
    ThumbnailLoader.centerImage(
        this.image_, fillMode, autoFillThreshold, boxWidth, boxHeight);

    if (this.image_.parentNode !== box) {
      box.textContent = '';
      box.appendChild(this.image_!);
    }

    if (!this.taskId_) {
      this.image_.classList.add('cached');
    }
  }

  /**
   * Updates the image style to fit/fill the container.
   *
   * Using webkit center packing does not align the image properly, so we need
   * to wait until the image loads and its dimensions are known, then manually
   * position it at the center.
   *
   * @param box Containing element.
   * @param img Element containing an image.
   * @param fillMode Fill mode.
   * @param autoFillThreshold Threshold value which is used for fill mode auto.
   * @param boxWidth Container box's width.
   * @param boxHeight Container box's height.
   */
  static centerImage(
      img: HTMLImageElement, fillMode: FillMode, autoFillThreshold: number,
      boxWidth: number, boxHeight: number) {
    const imageWidth = img.width;
    const imageHeight = img.height;

    let fractionX;
    let fractionY;

    let fill;
    switch (fillMode) {
      case FillMode.FILL:
      case FillMode.OVER_FILL:
        fill = true;
        break;
      case FillMode.FIT:
        fill = false;
        break;
      case FillMode.AUTO:
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

      if (fillMode !== FillMode.OVER_FILL) {
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

    function percent(fraction: number) {
      return (fraction * 100).toFixed(2) + '%';
    }

    img.style.width = percent(fractionX);
    img.style.height = percent(fractionY);
    img.style.left = percent((1 - fractionX) / 2);
    img.style.top = percent((1 - fractionY) / 2);
  }
}

/**
 * In percents (0.0 - 1.0), how much area can be cropped to fill an image
 * in a container, when loading a thumbnail in FillMode.AUTO mode.
 * The default 30% value allows to fill 16:9, 3:2 pictures in 4:3 element.
 */
const AUTO_FILL_THRESHOLD_DEFAULT_VALUE = 0.3;

/**
 * Type of displaying a thumbnail within a box.
 */
export enum FillMode {
  FILL = 0,   // Fill whole box. Image may be cropped.
  FIT,        // Keep aspect ratio, do not crop.
  OVER_FILL,  // Fill whole box with possible stretching.
  AUTO,       // Try to fill, but if incompatible aspect ratio, then fit.
}

/**
 * Load target of ThumbnailLoader.
 */
export enum LoadTarget {
  // e.g. Drive thumbnail, FSP thumbnail.
  EXTERNAL_METADATA = 'externalMetadata',
  // e.g. EXIF thumbnail.
  CONTENT_METADATA = 'contentMetadata',
  // Image file itself.
  FILE_ENTRY = 'fileEntry',
}

/**
 * Maximum thumbnail's width when generating from the full resolution image.
 */
export const THUMBNAIL_MAX_WIDTH = 500;

/**
 * Maximum thumbnail's height when generating from the full resolution image.
 */
export const THUMBNAIL_MAX_HEIGHT = 500;
