// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-picture-list' is a Polymer element used to show a selectable list of
 * profile pictures plus a camera selector, file picker, and the current
 * profile image.
 */

Polymer({
  is: 'cr-picture-list',

  behaviors: [CrPngBehavior],

  properties: {
    cameraPresent: Boolean,

    /**
     * The default user images.
     * @type {!Array<!{index: number, title: string, url: string}>}
     */
    defaultImages: {
      type: Array,
      observer: 'onDefaultImagesChanged_',
    },

    /** Strings provided by host */
    chooseFileLabel: String,
    oldImageLabel: String,
    profileImageLabel: String,
    takePhotoLabel: String,

    /**
     * The currently selected item. This property is bound to the iron-selector
     * and never directly assigned. This may be undefined momentarily as
     * the selection changes due to iron-selector implementation details.
     * @private {?CrPicture.ImageElement}
     */
    selectedItem: {
      type: Object,
      notify: true,
      observer: 'onImageSelected_',
    },

    /**
     * The url of the currently set profile picture image.
     * @private
     */
    profileImageUrl_: {
      type: String,
      value: CrPicture.kDefaultImageUrl,
    },

    /**
     * The url of the old image, which is either the existing image sourced from
     * the camera, a file, or a deprecated default image.
     * @private
     */
    oldImageUrl_: {
      type: String,
      value: '',
    },

    /**
     * The index associated with the old image if it was a default image, or -1
     * if the old image was not a defaul timage (i.e. a camera or file image).
     * @private
     */
    oldImageIndex_: {
      type: Number,
      value: -1,
    },

    /** @private */
    selectionTypesEnum_: {
      type: Object,
      value: CrPicture.SelectionTypes,
      readOnly: true,
    },
  },

  /** @private {boolean} */
  cameraSelected_: false,

  /** @private {string} */
  selectedImageUrl_: '',

  /**
   * The fallback image to be selected when the user discards the 'old' image.
   * This may be null if the user started with the old image.
   * @private {?CrPicture.ImageElement}
   */
  fallbackImage_: null,

  setFocus: function() {
    if (this.selectedItem) {
      this.selectedItem.focus();
    }
  },

  onImageSelected_: function(newImg, oldImg) {
    if (newImg) {
      newImg.setAttribute('tabindex', '0');
      newImg.setAttribute('aria-checked', 'true');
      newImg.focus();
    }
    if (oldImg) {
      oldImg.removeAttribute('tabindex');
      oldImg.removeAttribute('aria-checked');
    }
  },

  /**
   * @param {string} imageUrl
   * @param {boolean} selected
   */
  setProfileImageUrl: function(imageUrl, selected) {
    this.profileImageUrl_ = imageUrl;
    this.$.profileImage.title = this.profileImageLabel;
    if (!selected) {
      return;
    }
    this.setSelectedImage_(this.$.profileImage);
  },

  /**
   * @param {string} imageUrl
   */
  setSelectedImageUrl(imageUrl) {
    const image = this.$.selector.items.find(function(image) {
      return image.dataset.url == imageUrl;
    });
    if (image) {
      this.setSelectedImage_(image);
      this.selectedImageUrl_ = '';
    } else {
      this.selectedImageUrl_ = imageUrl;
    }
  },

  /**
   * @param {string} imageUrl
   * @param {number=} imageIndex
   */
  setOldImageUrl(imageUrl, imageIndex) {
    if (imageUrl == CrPicture.kDefaultImageUrl || imageIndex === 0) {
      // Treat the default image as empty so it does not show in the list.
      this.oldImageUrl_ = '';
      this.setSelectedImageUrl(CrPicture.kDefaultImageUrl);
      return;
    }
    this.oldImageUrl_ = imageUrl;
    this.oldImageIndex_ = imageIndex === undefined ? -1 : imageIndex;
    if (imageUrl) {
      this.$.selector.select(this.$.selector.indexOf(this.$.oldImage));
      this.selectedImageUrl_ = imageUrl;
    } else if (this.cameraSelected_) {
      this.$.selector.select(this.$.selector.indexOf(this.$.cameraImage));
    } else if (
        this.fallbackImage_ &&
        this.fallbackImage_.dataset.type != CrPicture.SelectionTypes.OLD) {
      this.selectImage_(this.fallbackImage_, true /* activate */);
    } else {
      this.selectImage_(this.$.profileImage, true /* activate */);
    }
  },

  /**
   * Handler for when accessibility-specific keys are pressed.
   * @param {!CustomEvent<!{key: string, keyboardEvent: Object}>} e
   */
  onKeysPressed: function(e) {
    if (!this.selectedItem) {
      return;
    }

    const selector = /** @type {IronSelectorElement} */ (this.$.selector);
    const prevSelected = this.selectedItem;
    let activate = false;
    switch (e.detail.key) {
      case 'enter':
      case 'space':
        activate = true;
        break;
      case 'up':
      case 'left':
        do {
          selector.selectPrevious();
        } while (this.selectedItem.hidden && this.selectedItem != prevSelected);
        break;
      case 'down':
      case 'right':
        do {
          selector.selectNext();
        } while (this.selectedItem.hidden && this.selectedItem != prevSelected);
        break;
      default:
        return;
    }
    this.selectImage_(this.selectedItem, activate);
    e.detail.keyboardEvent.preventDefault();
  },

  /**
   * @param {!CrPicture.ImageElement} image
   */
  setSelectedImage_(image) {
    this.fallbackImage_ = image;
    // If the user is currently taking a photo, do not change the focus.
    if (!this.selectedItem ||
        this.selectedItem.dataset.type != CrPicture.SelectionTypes.CAMERA) {
      this.$.selector.select(this.$.selector.indexOf(image));
      this.selectedItem = image;
    }
  },

  /** @private */
  onDefaultImagesChanged_: function() {
    if (this.selectedImageUrl_) {
      this.setSelectedImageUrl(this.selectedImageUrl_);
    }
  },

  /**
   * @param {!CrPicture.ImageElement} selected
   * @param {boolean} activate
   * @private
   */
  selectImage_(selected, activate) {
    this.cameraSelected_ =
        selected.dataset.type == CrPicture.SelectionTypes.CAMERA;
    this.selectedItem = selected;

    if (selected.dataset.type == CrPicture.SelectionTypes.CAMERA) {
      if (activate) {
        this.fire('focus-action', selected);
      }
    } else if (
        activate || selected.dataset.type != CrPicture.SelectionTypes.FILE) {
      this.fire('image-activate', selected);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronActivate_: function(event) {
    event.stopPropagation();
    const type = event.detail.item.dataset.type;
    // Don't change focus when activating the camera via mouse.
    const activate = type != CrPicture.SelectionTypes.CAMERA;
    this.selectImage_(event.detail.item, activate);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronSelect_: function(event) {
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelectedItemChanged_: function(event) {
    if (event.target.selectedItem) {
      event.target.selectedItem.scrollIntoViewIfNeeded(false);
    }
  },

  /**
   * Returns the image to use for 'src'.
   * @param {string} url
   * @return {string}
   * @private
   */
  getImgSrc_: function(url) {
    // Use first frame of animated user images.
    if (url.startsWith('chrome://theme')) {
      return url + '[0]';
    }

    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    if (url.split(',')[0] == 'data:image/png;base64') {
      return CrPngBehavior.convertImageSequenceToPng([url]);
    }

    return url;
  },

  /**
   * Returns the 2x (high dpi) image to use for 'srcset' for chrome://theme
   * images. Note: 'src' will still be used as the 1x candidate as per the HTML
   * spec.
   * @param {string} url
   * @return {string}
   * @private
   */
  getImgSrc2x_: function(url) {
    if (!url.startsWith('chrome://theme')) {
      return '';
    }
    return url + '[0]@2x 2x';
  },
});
