// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './files_metadata_entry.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExifTag} from '../js/metadata/exif_constants.js';

import {getTemplate} from './files_metadata_box.html.js';

export interface FilesMetadataBox {
  // File path and type, e.g. image, video.
  filePath: string;
  type: string;

  // File size, modification time, mimeType, location.
  size?: string;
  modificationTime: string;
  mediaMimeType: string;
  fileLocation: string;

  // True if the size field is loading.
  isSizeLoading?: boolean;

  // File-specific metadata.
  ifd?: Ifd;
  imageWidth: number;
  imageHeight: number;
  mediaAlbum: string;
  mediaArtist: string;
  mediaDuration: number;
  mediaGenre: string;
  mediaTitle: string;
  mediaTrack: string;
  mediaYearRecorded: string;
  originalLocation: string;

  hasFileSpecificMetadata_: boolean;
  metadata: string;
}

// TODO(b/289003444): Move the following type definitions into ../js/metadata
// when that folder has been converted to typescript.
type LatLongArray = [[number, number], [number, number], [number, number]];

// GPS Data as per https://exiv2.org/tags.html.
interface ExifData<T> {
  id: number;
  format: number;
  componentCount: number;
  value: T;
}

type ExifRationalData = ExifData<[number, number]>;

export interface RawIfd {
  cameraMaker: string;
  cameraModel: string;
  aperture: number;
  focalLength: number;
  exposureTime: number;
  isoSpeed: number;
  width: number;
  height: number;
  orientation: number;
  colorSpace: string;
  date: string;
}

interface Ifd {
  raw?: RawIfd;
  image?: {[K in ExifTag]: {value: string}};
  gps?: [
    ExifData<[number, number, number, number]>,  // GPS Version number.
    ExifData<string>,        // Latitude reference (i.e. N or S).
    ExifData<LatLongArray>,  // Latitude in degrees, minutes, and
                             // seconds.
    ExifData<string>,        // Longtitude reference (i.e. E or W).
    ExifData<LatLongArray>,  // Longtitude in degrees, minutes, and
                             // seconds.
  ];
  exif?: {
    [ExifTag.APERTURE]: ExifRationalData,
    [ExifTag.EXPOSURE_TIME]: ExifRationalData,
    [ExifTag.FOCAL_LENGTH]: ExifRationalData,
    [ExifTag.ISO_SPEED]: ExifRationalData,
    [ExifTag.DATETIME_ORIGINAL]: ExifData<string>,
    [ExifTag.CREATE_DATETIME]: ExifData<string>,
  };
}

export class FilesMetadataBox extends PolymerElement {
  static get is() {
    return 'files-metadata-box';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
       */
      hasFileSpecificMetadata_: Boolean,

      /**
       * FilesMetadataBox [metadata] attribute. Used to indicate the
       * metadata box field rendering phases.
       */
      metadata: {
        type: String,
        reflectToAttribute: true,
      },
    };
  }


  /**
   * Clears fields.
   *
   * @param keepSizeFields Do not clear size and isSizeLoading fields.
   */
  clear(keepSizeFields: boolean) {
    const reset: Partial<FilesMetadataBox> = {
      type: '',
      filePath: '',
      modificationTime: '',
      hasFileSpecificMetadata_: false,
      mediaMimeType: '',
      fileLocation: '',
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
  }

  isImage(type: string) {
    return type === 'image';
  }

  isVideo(type: string) {
    return type === 'video';
  }

  isAudio(type: string) {
    return type === 'audio';
  }

  /**
   * If the originalLocation is set, the preview is for a trashed item.
   */
  isTrashEntry(originalLocation: string|undefined) {
    return !(originalLocation && originalLocation.length > 0);
  }

  /**
   * Sets this.hasFileSpecificMetadata_ if there is file-specific metadata.
   */
  private setFileSpecificMetadata_() {
    this.hasFileSpecificMetadata_ =
        !!(this.imageWidth && this.imageHeight || this.mediaTitle ||
           this.mediaArtist || this.mediaAlbum || this.mediaDuration ||
           this.mediaGenre || this.mediaTrack || this.mediaYearRecorded ||
           this.ifd);
    return this.hasFileSpecificMetadata_;
  }

  /**
   * Sets the file |type| field based on setFileSpecificMetadata_().
   */
  setFileTypeInfo(type: string) {
    this.type = this.setFileSpecificMetadata_() ? type : '';
  }

  /**
   * Update the metadata attribute with the rendered metadata |type|.
   */
  metadataRendered(type?: string) {
    if (!type) {
      this.metadata = '';
    } else if (!this.metadata) {
      this.metadata = type;
    } else {
      this.metadata += ' ' + type;
    }
  }

  /**
   * Converts the duration into human friendly string.
   */
  time2string(time: string) {
    if (!time) {
      return '';
    }

    const parsedTime = parseInt(time, 10);
    const seconds = parsedTime % 60;
    const minutes = Math.floor(parsedTime / 60) % 60;
    const hours = Math.floor(parsedTime / 60 / 60);

    if (hours === 0) {
      return minutes + ':' + ('0' + seconds).slice(-2);
    }
    return hours + ':' + ('0' + minutes).slice(-2) + ('0' + seconds).slice(-2);
  }

  dimension(imageWidth: number, imageHeight: number) {
    if (imageWidth && imageHeight) {
      return imageWidth + ' x ' + imageHeight;
    }
    return '';
  }

  deviceModel(ifd: Ifd) {
    if (!ifd) {
      return '';
    }

    if (ifd.raw) {
      return ifd.raw.cameraModel || '';
    }

    const id = ExifTag.MODEL;
    const model = (ifd.image && ifd.image[id] && ifd.image[id].value) || '';
    return model.replace(/\0+$/, '').trim();
  }

  /**
   * @param r An array of two strings representing the numerator and the
   *     denominator.
   */
  private parseRational_(r: [number, number]) {
    const num = parseInt(r[0].toString(), 10);
    const den = parseInt(r[1].toString(), 10);
    return num / den;
  }

  /**
   * Returns geolocation as a string in the form of 'latitude, longitude',
   * where the values have 3 decimal precision. Negative latitude indicates
   * south latitude and negative longitude indicates west longitude.
   */
  geography(ifd: Ifd) {
    const gps = ifd && ifd.gps;
    if (!gps || !gps[1] || !gps[2] || !gps[3] || !gps[4]) {
      return '';
    }

    const computeCoordinate = (value: LatLongArray) => {
      return this.parseRational_(value[0]) +
          this.parseRational_(value[1]) / 60 +
          this.parseRational_(value[2]) / 3600;
    };

    const latitude =
        computeCoordinate(gps[2].value) * (gps[1].value === 'N\0' ? 1 : -1);
    const longitude =
        computeCoordinate(gps[4].value) * (gps[3].value === 'E\0' ? 1 : -1);

    return Number(latitude).toFixed(3) + ', ' + Number(longitude).toFixed(3);
  }

  /**
   * Returns device settings as a string containing the fNumber, exposureTime,
   * focalLength, and isoSpeed. Example: 'f/2.8 0.008 23mm ISO4000'.
   */
  deviceSettings(ifd: Ifd) {
    let result = '';

    if (ifd && ifd.raw) {
      result = this.rawDeviceSettings_(ifd.raw);
    } else if (ifd) {
      result = this.ifdDeviceSettings_(ifd);
    }

    return result;
  }

  dateTaken(ifd: Ifd) {
    // Exif has 2 fields that might contain the date.
    const d1 = ifd?.exif?.[ExifTag.DATETIME_ORIGINAL]?.value;
    const d2 = ifd?.exif?.[ExifTag.CREATE_DATETIME]?.value;
    // From PIEX.
    const d3 = ifd?.raw?.date;
    return d1 ?? d2 ?? d3 ?? '';
  }

  private rawDeviceSettings_(raw: Ifd['raw']) {
    let result = '';

    const aperture = raw?.aperture || 0;
    if (aperture) {
      result += 'f/' + aperture + ' ';
    }

    const exposureTime = raw?.exposureTime || 0;
    if (exposureTime) {
      result += exposureTime + ' ';
    }

    const focalLength = raw?.focalLength || 0;
    if (focalLength) {
      result += focalLength + 'mm ';
    }

    const isoSpeed = raw?.isoSpeed || 0;
    if (isoSpeed) {
      result += 'ISO' + isoSpeed + ' ';
    }

    return result.trimEnd();
  }

  private ifdDeviceSettings_(ifd: Ifd) {
    const exif = ifd.exif;
    if (!exif) {
      return '';
    }

    function parseExifNumber(field: ExifRationalData) {
      let number = 0;

      if (field && field.value) {
        if (Array.isArray(field.value)) {
          const denominator = parseInt(field.value[1].toString(), 10);
          if (denominator) {
            number = parseInt(field.value[0].toString(), 10) / denominator;
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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'files-metadata-box': FilesMetadataBox;
  }
}

customElements.define(FilesMetadataBox.is, FilesMetadataBox);
