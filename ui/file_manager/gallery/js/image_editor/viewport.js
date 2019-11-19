// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Formats string by replacing place holder with actual values.
 * @param {string} str String includes placeholder '$n'. n starts from 1.
 * @param {...*} var_args Values inserted into the place holders.
 * @return {string}
 */
function formatString(str, var_args) {
  var args = arguments;
  return str.replace(/\$[1-9]/g, function(placeHolder) {
    return args[placeHolder[1]];
  });
}

/**
 * Viewport class controls the way the image is displayed (scale, offset etc).
 * Screen size is same with window size by default. Change screen size and
 * position by changing screenTop and screenBottom.
 */
class Viewport extends cr.EventTarget {
  /**
   * @param {!Window} targetWindow A window which this viewport is attached to.
   */
  constructor(targetWindow) {
    super();

    /**
     * Window
     * @private {!Window}
     */
    this.window_ = targetWindow;

    /**
     * Size of the full resolution image.
     * @type {!ImageRect}
     * @private
     */
    this.imageBounds_ = new ImageRect(0, 0, 0, 0);

    /**
     * Bounds of the image element on screen without zoom and offset.
     * @type {ImageRect}
     * @private
     */
    this.imageElementBoundsOnScreen_ = null;

    /**
     * Bounds of the image with zoom and offset.
     * @type {ImageRect}
     * @private
     */
    this.imageBoundsOnScreen_ = null;

    /**
     * Image bounds that is clipped with the screen bounds.
     * @type {ImageRect}
     * @private
     */
    this.imageBoundsOnScreenClipped_ = null;

    /**
     * Scale from the full resolution image to the screen displayed image. This
     * is not zoom operated by users.
     * @type {number}
     * @private
     */
    this.scale_ = 1;

    /**
     * Zoom ratio specified by user operations.
     * @type {number}
     * @private
     */
    this.zoom_ = 1;

    /**
     * Offset specified by user operations.
     * @type {number}
     * @private
     */
    this.offsetX_ = 0;

    /**
     * Offset specified by user operations.
     * @type {number}
     * @private
     */
    this.offsetY_ = 0;

    /**
     * Integer Rotation value.
     * The rotation angle is this.rotation_ * 90.
     * @type {number}
     * @private
     */
    this.rotation_ = 0;

    /**
     * Generation of the screen size image cache.
     * This is incremented every time when the size of image cache is changed.
     * @type {number}
     * @private
     */
    this.generation_ = 0;

    /**
     * Top margin of the screen.
     * @private {number}
     */
    this.screenTop_ = 0;

    /**
     * Bottom margin of the screen.
     * @private {number}
     */
    this.screenBottom_ = 0;

    /**
     * Window width.
     * @private {number}
     */
    this.windowWidth_ = this.window_.innerWidth;

    /**
     * Window height.
     * @private {number}
     */
    this.windowHeight_ = this.window_.innerHeight;

    this.window_.addEventListener('resize', this.onWindowResize_.bind(this));

    this.update_();
  }

  /**
   * Sets image size.
   * @param {number} width Image width.
   * @param {number} height Image height.
   */
  setImageSize(width, height) {
    this.imageBounds_ = ImageRect.createFromWidthAndHeight(width, height);
    this.update_();
  }

  /**
   * Handles window resize event.
   */
  onWindowResize_(event) {
    this.windowWidth_ = event.target.innerWidth;
    this.windowHeight_ = event.target.innerHeight;
    this.update_();

    // Dispatches resize event of viewport.
    var resizeEvent = new Event('resize');
    this.dispatchEvent(resizeEvent);
  }

  /**
   * Sets screen top.
   * @param {number} top Top.
   */
  setScreenTop(top) {
    this.screenTop_ = top;
    this.update_();
  }

  /**
   * Sets screen bottom.
   * @param {number} bottom Bottom.
   */
  setScreenBottom(bottom) {
    this.screenBottom_ = bottom;
    this.update_();
  }

  /**
   * Sets zoom value directly.
   * @param {number} zoom New zoom value.
   */
  setZoom(zoom) {
    var zoomMin = Viewport.ZOOM_RATIOS[0];
    var zoomMax = Viewport.ZOOM_RATIOS[Viewport.ZOOM_RATIOS.length - 1];
    var adjustedZoom = Math.max(zoomMin, Math.min(zoom, zoomMax));
    this.zoom_ = adjustedZoom;
    this.update_();
  }

  /**
   * Returns the value of zoom.
   * @return {number} Zoom value.
   */
  getZoom() {
    return this.zoom_;
  }

  /**
   * Sets the nearest larger value of ZOOM_RATIOS.
   */
  zoomIn() {
    var zoom = Viewport.ZOOM_RATIOS[0];
    for (var i = 0; i < Viewport.ZOOM_RATIOS.length; i++) {
      zoom = Viewport.ZOOM_RATIOS[i];
      if (zoom > this.zoom_) {
        break;
      }
    }
    this.setZoom(zoom);
  }

  /**
   * Sets the nearest smaller value of ZOOM_RATIOS.
   */
  zoomOut() {
    var zoom = Viewport.ZOOM_RATIOS[Viewport.ZOOM_RATIOS.length - 1];
    for (var i = Viewport.ZOOM_RATIOS.length - 1; i >= 0; i--) {
      zoom = Viewport.ZOOM_RATIOS[i];
      if (zoom < this.zoom_) {
        break;
      }
    }
    this.setZoom(zoom);
  }

  /**
   * Obtains whether the picture is zoomed or not.
   * @return {boolean}
   */
  isZoomed() {
    return this.zoom_ !== 1;
  }

  /**
   * Sets the rotation value.
   * @param {number} rotation New rotation value.
   */
  setRotation(rotation) {
    this.rotation_ = rotation;
    this.update_();
  }

  /**
   * Obtains the rotation value.
   * @return {number} Current rotation value.
   */
  getRotation() {
    return this.rotation_;
  }

  /**
   * Returns image scale so that it matches screen size as long as it does not
   * exceed maximum size.
   *
   * @param {number} width Width of image.
   * @param {number} height Height of image.
   * @param {number} maxWidth Max width of image.
   * @param {number} maxHeight Max height of image.
   * @return {number} The ratio of the full resotion image size and the
   *     calculated displayed image size.
   * @private
   */
  getFittingScaleForImageSize_(width, height, maxWidth, maxHeight) {
    return Math.min(
        maxWidth / width, maxHeight / height,
        this.getScreenBounds().width / width,
        this.getScreenBounds().height / height);
  }

  /**
   * Returns offset X.
   * @return {number} X-offset of the viewport.
   */
  getOffsetX() {
    return this.offsetX_;
  }

  /**
   * Returns offset Y.
   * @return {number} Y-offset of the viewport.
   */
  getOffsetY() {
    return this.offsetY_;
  }

  /**
   * Set the image offset in the viewport.
   * @param {number} x X-offset.
   * @param {number} y Y-offset.
   */
  setOffset(x, y) {
    if (this.offsetX_ == x && this.offsetY_ == y) {
      return;
    }
    this.offsetX_ = x;
    this.offsetY_ = y;
    this.update_();
  }

  /**
   * Returns image bounds.
   * @return {!ImageRect} The image bounds in image coordinates.
   */
  getImageBounds() {
    return this.imageBounds_;
  }

  /**
   * Returns screen bounds.
   * @return {!ImageRect} The screen bounds in screen coordinates.
   */
  getScreenBounds() {
    return new ImageRect(
        0, this.screenTop_, this.windowWidth_,
        this.windowHeight_ - this.screenTop_ - this.screenBottom_);
  }

  /**
   * Returns device bounds.
   * @return {!ImageRect} The size of screen cache canvas.
   */
  getDeviceBounds() {
    return ImageRect.createFromWidthAndHeight(
        this.imageElementBoundsOnScreen_.width * window.devicePixelRatio,
        this.imageElementBoundsOnScreen_.height * window.devicePixelRatio);
  }

  /**
   * A counter that is incremented with each viewport state change.
   * Clients that cache anything that depends on the viewport state should keep
   * track of this counter.
   * @return {number} counter.
   */
  getCacheGeneration() {
    return this.generation_;
  }

  /**
   * Returns image bounds in screen coordinates.
   * @return {!ImageRect} The image bounds in screen coordinates.
   */
  getImageBoundsOnScreen() {
    assert(this.imageBoundsOnScreen_);
    return this.imageBoundsOnScreen_;
  }

  /**
   * The image bounds on screen, which is clipped with the screen size.
   * @return {!ImageRect}
   */
  getImageBoundsOnScreenClipped() {
    assert(this.imageBoundsOnScreenClipped_);
    return this.imageBoundsOnScreenClipped_;
  }

  /**
   * Returns size in image coordinates.
   * @param {number} size Size in screen coordinates.
   * @return {number} Size in image coordinates.
   */
  screenToImageSize(size) {
    return size / this.scale_;
  }

  /**
   * Returns X in image coordinates.
   * @param {number} x X in screen coordinates.
   * @return {number} X in image coordinates.
   */
  screenToImageX(x) {
    return Math.round((x - this.imageBoundsOnScreen_.left) / this.scale_);
  }

  /**
   * Returns Y in image coordinates.
   * @param {number} y Y in screen coordinates.
   * @return {number} Y in image coordinates.
   */
  screenToImageY(y) {
    return Math.round((y - this.imageBoundsOnScreen_.top) / this.scale_);
  }

  /**
   * Returns a rectangle in image coordinates.
   * @param {!ImageRect} rect Rectangle in screen coordinates.
   * @return {!ImageRect} Rectangle in image coordinates.
   */
  screenToImageRect(rect) {
    return new ImageRect(
        this.screenToImageX(rect.left), this.screenToImageY(rect.top),
        this.screenToImageSize(rect.width),
        this.screenToImageSize(rect.height));
  }

  /**
   * Returns size in screen coordinates.
   * @param {number} size Size in image coordinates.
   * @return {number} Size in screen coordinates.
   */
  imageToScreenSize(size) {
    return size * this.scale_;
  }

  /**
   * Returns X in screen coordinates.
   * @param {number} x X in image coordinates.
   * @return {number} X in screen coordinates.
   */
  imageToScreenX(x) {
    return Math.round(this.imageBoundsOnScreen_.left + x * this.scale_);
  }

  /**
   * Returns Y in screen coordinates.
   * @param {number} y Y in image coordinates.
   * @return {number} Y in screen coordinates.
   */
  imageToScreenY(y) {
    return Math.round(this.imageBoundsOnScreen_.top + y * this.scale_);
  }

  /**
   * Returns a rectangle in screen coordinates.
   * @param {!ImageRect} rect Rectangle in image coordinates.
   * @return {!ImageRect} Rectangle in screen coordinates.
   */
  imageToScreenRect(rect) {
    return new ImageRect(
        this.imageToScreenX(rect.left), this.imageToScreenY(rect.top),
        Math.round(this.imageToScreenSize(rect.width)),
        Math.round(this.imageToScreenSize(rect.height)));
  }

  /**
   * Returns a rectangle with given geometry.
   * @param {number} width Width of the rectangle.
   * @param {number} height Height of the rectangle.
   * @param {number} offsetX X-offset of center position of the rectangle.
   * @param {number} offsetY Y-offset of center position of the rectangle.
   * @return {!ImageRect} Rectangle with given geometry.
   * @private
   */
  getCenteredRect_(width, height, offsetX, offsetY) {
    var screenBounds = this.getScreenBounds();
    return new ImageRect(
        ~~((screenBounds.width - width) / 2) + offsetX,
        ~~((screenBounds.height - height) / 2) + screenBounds.top + offsetY,
        width, height);
  }

  /**
   * Resets zoom and offset.
   */
  resetView() {
    this.zoom_ = 1;
    this.offsetX_ = 0;
    this.offsetY_ = 0;
    this.rotation_ = 0;
    this.update_();
  }

  /**
   * Recalculate the viewport parameters.
   * @private
   */
  update_() {
    // Update scale.
    this.scale_ = this.getFittingScaleForImageSize_(
        this.imageBounds_.width, this.imageBounds_.height,
        this.imageBounds_.width, this.imageBounds_.height);

    // Limit offset values.
    var zoomedWidht;
    var zoomedHeight;
    if (this.rotation_ % 2 == 0) {
      zoomedWidht = ~~(this.imageBounds_.width * this.scale_ * this.zoom_);
      zoomedHeight = ~~(this.imageBounds_.height * this.scale_ * this.zoom_);
    } else {
      var scale = this.getFittingScaleForImageSize_(
          this.imageBounds_.height, this.imageBounds_.width,
          this.imageBounds_.height, this.imageBounds_.width);
      zoomedWidht = ~~(this.imageBounds_.height * scale * this.zoom_);
      zoomedHeight = ~~(this.imageBounds_.width * scale * this.zoom_);
    }
    var dx = Math.max(zoomedWidht - this.getScreenBounds().width, 0) / 2;
    var dy = Math.max(zoomedHeight - this.getScreenBounds().height, 0) / 2;
    this.offsetX_ = ImageUtil.clamp(-dx, this.offsetX_, dx);
    this.offsetY_ = ImageUtil.clamp(-dy, this.offsetY_, dy);

    // Image bounds on screen.
    this.imageBoundsOnScreen_ = this.getCenteredRect_(
        zoomedWidht, zoomedHeight, this.offsetX_, this.offsetY_);

    // Image bounds of element (that is not applied zoom and offset) on screen.
    var oldBounds = this.imageElementBoundsOnScreen_;
    this.imageElementBoundsOnScreen_ = this.getCenteredRect_(
        ~~(this.imageBounds_.width * this.scale_),
        ~~(this.imageBounds_.height * this.scale_), 0, 0);
    if (!oldBounds ||
        this.imageElementBoundsOnScreen_.width != oldBounds.width ||
        this.imageElementBoundsOnScreen_.height != oldBounds.height) {
      this.generation_++;
    }

    // Image bounds on screen clipped with the screen bounds.
    var left = Math.max(this.imageBoundsOnScreen_.left, 0);
    var top = Math.max(this.imageBoundsOnScreen_.top, this.screenTop_);
    var right =
        Math.min(this.imageBoundsOnScreen_.right, this.getScreenBounds().width);
    var bottom = Math.min(
        this.imageBoundsOnScreen_.bottom,
        this.getScreenBounds().height + this.screenTop_);
    this.imageBoundsOnScreenClipped_ =
        new ImageRect(left, top, right - left, bottom - top);
  }

  /**
   * Clones the viewport.
   * @return {!Viewport} New instance.
   */
  clone() {
    var viewport = new Viewport(this.window_);
    viewport.imageBounds_ = ImageRect.createFromBounds(this.imageBounds_);
    viewport.scale_ = this.scale_;
    viewport.zoom_ = this.zoom_;
    viewport.offsetX_ = this.offsetX_;
    viewport.offsetY_ = this.offsetY_;
    viewport.screenTop_ = this.screenTop_;
    viewport.screenBottom_ = this.screenBottom_;
    viewport.windowWidth_ = this.windowWidth_;
    viewport.windowHeight_ = this.windowHeight_;
    viewport.rotation_ = this.rotation_;
    viewport.generation_ = this.generation_;
    viewport.update_();
    return viewport;
  }

  /**
   * Obtains CSS transformation string that matches the image dimension with
   * |screenRect|.
   * @param {number} width Width of image.
   * @param {number} height Height of image.
   * @param {!ImageRect} screenRect Rectangle in window coordinate system. The
   *     origin of the coordinate system is located at the left upper of the
   *     window.
   */
  getScreenRectTransformation(width, height, screenRect) {
    var dx = screenRect.left + (screenRect.width - width) / 2;
    var dy = screenRect.top + (screenRect.height - height) / 2;

    return formatString(
        'translate($1px,$2px) scale($3,$4)', dx, dy, screenRect.width / width,
        screenRect.height / height);
  }

  /**
   * Obtains CSS transformation string that places the cropped image at the
   * original position in the whole image.
   * @param {number} width Width of cropped image.
   * @param {number} height Width of cropped image.
   * @param {number} wholeWidthMax Max width value that is used for layouting
   *     whole image.
   * @param {number} wholeHeightMax Max height value that is used for layouting
   *     whole image.
   * @param {!ImageRect} cropRect Crop rectangle in the whole image. The origin
   *     is left upper of the whole image.
   */
  getCroppingTransformation(
      width, height, wholeWidthMax, wholeHeightMax, cropRect) {
    var fittingScale = this.getFittingScaleForImageSize_(
        wholeWidthMax, wholeHeightMax, wholeWidthMax, wholeHeightMax);
    var wholeWidth = wholeWidthMax * fittingScale;
    var wholeHeight = wholeHeightMax * fittingScale;
    var wholeRect = this.getCenteredRect_(wholeWidth, wholeHeight, 0, 0);
    return this.getScreenRectTransformation(
        width, height,
        new ImageRect(
            wholeRect.left + cropRect.left * fittingScale,
            wholeRect.top + cropRect.top * fittingScale,
            cropRect.width * fittingScale, cropRect.height * fittingScale));
  }

  /**
   * Obtains CSS transformation for the screen image.
   * @param {number} width Width of image.
   * @param {number} height Height of image.
   * @param {number=} opt_dx Amount of horizontal shift.
   * @return {string} Transformation description.
   */
  getTransformation(width, height, opt_dx) {
    return this.getTransformationInternal_(
        width, height, this.rotation_, this.zoom_,
        this.offsetX_ + (opt_dx || 0), this.offsetY_);
  }

  /**
   * Obtains CSS transformation that makes the rotated image fit the original
   * image. The new rotated image that the transformation is applied to looks
   * the same with original image.
   *
   * @param {number} width Width of image.
   * @param {number} height Height of image.
   * @param {number} rotation Number of clockwise 90 degree rotation. The
   *     rotation angle of the image is rotation * 90.
   * @return {string} Transformation description.
   */
  getRotatingTransformation(width, height, rotation) {
    return this.getTransformationInternal_(width, height, rotation, 1, 0, 0);
  }

  /**
   * Obtains CSS transformation that placed the image in the application window.
   * @param {number} width Width of image.
   * @param {number} height Height of image.
   * @param {number} rotation Number of clockwise 90 degree rotation. The
   *     rotation angle of the image is rotation * 90.
   * @param {number} zoom Zoom rate.
   * @param {number} offsetX Horizontal offset.
   * @param {number} offsetY Vertical offset.
   * @private
   */
  getTransformationInternal_(width, height, rotation, zoom, offsetX, offsetY) {
    var rotatedWidth = rotation % 2 ? height : width;
    var rotatedHeight = rotation % 2 ? width : height;
    var rotatedMaxWidth =
        rotation % 2 ? this.imageBounds_.height : this.imageBounds_.width;
    var rotatedMaxHeight =
        rotation % 2 ? this.imageBounds_.width : this.imageBounds_.height;

    // Scale.
    var fittingScale = this.getFittingScaleForImageSize_(
        rotatedWidth, rotatedHeight, rotatedMaxWidth, rotatedMaxHeight);

    // Offset for centering.
    var rect = this.getCenteredRect_(width, height, offsetX, offsetY);
    return formatString(
        'translate($1px,$2px) scale($3) rotate($4deg)', rect.left, rect.top,
        fittingScale * zoom, rotation * 90);
  }
}

/**
 * Zoom ratios.
 *
 * @type {Array<number>}
 * @const
 */
Viewport.ZOOM_RATIOS = [1, 1.5, 2, 3];
