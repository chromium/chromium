// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './files_metadata_entry.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,

  is: 'files-metadata-box',

  properties: {
    // File path and type, e.g. image, video.
    filePath: String,
    type: String,

    // File size, modification time, mimeType, location.
    size: String,
    modificationTime: String,
    mediaMimeType: String,
    fileLocation: String,

    // True if the size field is loading.
    isSizeLoading: Boolean,

    // File-specific metadata.
    /** @type {?Object} */
    ifd: Object,
    imageWidth: Number,
    imageHeight: Number,
    mediaAlbum: String,
    mediaArtist: String,
    mediaDuration: Number,
    mediaGenre: String,
    mediaTitle: String,
    mediaTrack: String,
    mediaYearRecorded: String,
    originalLocation: String,

    /**
     * True if the file has file-specific metadata.
     * @private
     */
    hasFileSpecificMetadata_: Boolean,

    /**
     * FilesMetadataBox [metadata] attribute. Used to indicate the metadata box
     * field rendering phases.
     * @private
     */
    metadata: {
      type: String,
      reflectToAttribute: true,
    },
  },

  /**
   * Clears fields.
   * @param {boolean} keepSizeFields do not clear size and isSizeLoading fields.
   */
  clear: function(keepSizeFields) {
    const reset = {
      type: '',
      filePath: '',
      modificationTime: '',
      hasFileSpecificMetadata_: false,
      mediaMimeType: '',
      fileLocation: '',
      ifd: null,
      imageWidth: 0,
      imageHeight: 0,
      mediaTitle: '',
      mediaArtist: '',
      mediaAlbum: '',
      mediaDuration: 0,
      mediaGenre: '',
      mediaTrack: '',
      mediaYearRecorded: '',
      metadata: '',
      originalLocation: '',
    };

    if (!keepSizeFields) {
      reset.isSizeLoading = false;
      reset.size = '';
    }

    this.setProperties(reset);
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isImage_: function(type) {
    return type === 'image';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isVideo_: function(type) {
    return type === 'video';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isAudio_: function(type) {
    return type === 'audio';
  },

  /**
   * If the originalLocation is set, the preview is for a trashed item.
   * @returns {boolean}
   */
  isTrashEntry: function(originalLocation) {
    return !(originalLocation && originalLocation.length > 0);
  },

  /**
   * Sets this.hasFileSpecificMetadata_ if there is file-specific metadata.
   * @return {boolean}
   *
   * @private
   */
  setFileSpecificMetadata_: function() {
    this.hasFileSpecificMetadata_ =
        !!(this.imageWidth && this.imageHeight || this.mediaTitle ||
           this.mediaArtist || this.mediaAlbum || this.mediaDuration ||
           this.mediaGenre || this.mediaTrack || this.mediaYearRecorded ||
           this.ifd);
    return this.hasFileSpecificMetadata_;
  },

  /**
   * Sets the file |type| field based on setFileSpecificMetadata_().
   * @public
   */
  setFileTypeInfo: function(type) {
    this.type = this.setFileSpecificMetadata_() ? type : '';
  },

  /**
   * Update the metadata attribute with the rendered metadata |type|.
   * @param {?string} type
   * @public
   */
  metadataRendered: function(type) {
    if (!type) {
      this.metadata = '';
    } else if (!this.metadata) {
      this.metadata = type;
    } else {
      this.metadata += ' ' + type;
    }
  },

  /**
   * Converts the duration into human friendly string.
   * @param {number} time the duration in seconds.
   * @return {string} String representation of the given duration.
   */
  time2string_: function(time) {
    if (!time) {
      return '';
    }

    time = parseInt(time, 10);
    const seconds = time % 60;
    const minutes = Math.floor(time / 60) % 60;
    const hours = Math.floor(time / 60 / 60);

    if (hours === 0) {
      return minutes + ':' + ('0' + seconds).slice(-2);
    }
    return hours + ':' + ('0' + minutes).slice(-2) + ('0' + seconds).slice(-2);
  },

  /**
   * @param {number} imageWidth
   * @param {number} imageHeight
   * @return {string}
   *
   * @private
   */
  dimension_: function(imageWidth, imageHeight) {
    if (imageWidth && imageHeight) {
      return imageWidth + ' x ' + imageHeight;
    }
    return '';
  },

  /**
   * @param {?Object} ifd
   * @return {string}
   *
   * @private
   */
  deviceModel_: function(ifd) {
    if (!ifd) {
      return '';
    }

    if (ifd['raw']) {
      return ifd['raw']['cameraModel'] || '';
    }

    const id = 272;
    const model = (ifd.image && ifd.image[id] && ifd.image[id].value) || '';
    return model.replace(/\0+$/, '').trim();
  },

  /**
   * @param {Array<String>} r array of two strings representing the numerator
   *     and the denominator.
   * @return {number}
   * @private
   */
  parseRational_: function(r) {
    const num = parseInt(r[0], 10);
    const den = parseInt(r[1], 10);
    return num / den;
  },

  /**
   * Returns geolocation as a string in the form of 'latitude, longitude',
   * where the values have 3 decimal precision. Negative latitude indicates
   * south latitude and negative longitude indicates west longitude.
   * @param {?Object} ifd
   * @return {string}
   *
   * @private
   */
  geography_: function(ifd) {
    const gps = ifd && ifd.gps;
    if (!gps || !gps[1] || !gps[2] || !gps[3] || !gps[4]) {
      return '';
    }

    const computeCoordinate = value => {
      return this.parseRational_(value[0]) +
          this.parseRational_(value[1]) / 60 +
          this.parseRational_(value[2]) / 3600;
    };

    const latitude =
        computeCoordinate(gps[2].value) * (gps[1].value === 'N\0' ? 1 : -1);
    const longitude =
        computeCoordinate(gps[4].value) * (gps[3].value === 'E\0' ? 1 : -1);

    return Number(latitude).toFixed(3) + ', ' + Number(longitude).toFixed(3);
  },

  /**
   * Returns device settings as a string containing the fNumber, exposureTime,
   * focalLength, and isoSpeed. Example: 'f/2.8 0.008 23mm ISO4000'.
   * @param {?Object} ifd
   * @return {string}
   *
   * @private
   */
  deviceSettings_: function(ifd) {
    let result = '';

    if (ifd && ifd['raw']) {
      result = this.rawDeviceSettings_(ifd['raw']);
    } else if (ifd) {
      result = this.ifdDeviceSettings_(ifd);
    }

    return result;
  },

  /**
   * @param {!Object} raw
   * @return {string}
   *
   * @private
   */
  rawDeviceSettings_: function(raw) {
    let result = '';

    const aperture = raw['aperture'] || 0;
    if (aperture) {
      result += 'f/' + aperture + ' ';
    }

    const exposureTime = raw['exposureTime'] || 0;
    if (exposureTime) {
      result += exposureTime + ' ';
    }

    const focalLength = raw['focalLength'] || 0;
    if (focalLength) {
      result += focalLength + 'mm ';
    }

    const isoSpeed = raw['isoSpeed'] || 0;
    if (isoSpeed) {
      result += 'ISO' + isoSpeed + ' ';
    }

    return result.trimEnd();
  },

  /**
   * @param {!Object} ifd
   * @return {string}
   *
   * @private
   */
  ifdDeviceSettings_: function(ifd) {
    const exif = ifd['exif'];
    if (!exif) {
      return '';
    }

    /**
     * @param {Object} field Exif field to be parsed as a number.
     * @return {number}
     */
    function parseExifNumber(field) {
      let number = 0;

      if (field && field.value) {
        if (Array.isArray(field.value)) {
          const denominator = parseInt(field.value[1], 10);
          if (denominator) {
            number = parseInt(field.value[0], 10) / denominator;
          }
        } else {
          number = parseInt(field.value, 10);
        }

        if (Number.isNaN(number)) {
          number = 0;
        } else if (!Number.isInteger(number)) {
          number = Number(number.toFixed(3).replace(/0+$/, ''));
        }
      }

      return number;
    }

    let result = '';

    const aperture = parseExifNumber(exif[33437]);
    if (aperture) {
      result += 'f/' + aperture + ' ';
    }

    const exposureTime = parseExifNumber(exif[33434]);
    if (exposureTime) {
      result += exposureTime + ' ';
    }

    const focalLength = parseExifNumber(exif[37386]);
    if (focalLength) {
      result += focalLength + 'mm ';
    }

    const isoSpeed = parseExifNumber(exif[34855]);
    if (isoSpeed) {
      result += 'ISO' + isoSpeed + ' ';
    }

    return result.trimEnd();
  },
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_metadata_box.js
