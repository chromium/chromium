// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Exif marks.
 */
export enum ExifMark {
  // Start of "stream" (the actual image data).
  SOS = 0xffda,
  // Start of "frame".
  SOF = 0xffc0,
  // Start of image data.
  SOI = 0xffd8,
  // End of image data.
  EOI = 0xffd9,
  // APP0 block, most commonly JFIF data.
  APP0 = 0xffe0,
  // Start of exif block.
  EXIF = 0xffe1,
}

/**
 * Exif align.
 */
export enum ExifAlign {
  // Indicates little endian exif data.
  LITTLE = 0x4949,
  // Indicates big endian exif data.
  BIG = 0x4d4d,
}

/**
 * Exif tag.
 */
export enum ExifTag {
  // First directory containing TIFF data.
  TIFF = 0x002a,
  // Pointer from TIFF to the GPS directory.
  GPSDATA = 0x8825,
  // Pointer from TIFF to the EXIF IFD.
  EXIFDATA = 0x8769,
  // Pointer from TIFF to thumbnail.
  JPG_THUMB_OFFSET = 0x0201,
  // Length of thumbnail data.
  JPG_THUMB_LENGTH = 0x0202,
  IMAGE_WIDTH = 0x0100,
  IMAGE_HEIGHT = 0x0101,
  COMPRESSION = 0x0102,
  MAKE = 0x010f,
  MODEL = 0x0110,
  ORIENTATION = 0x0112,
  MODIFIED_DATETIME = 0x132,
  X_DIMENSION = 0xA002,
  Y_DIMENSION = 0xA003,
  SOFTWARE = 0x0131,
  APERTURE = 0x829D,
  EXPOSURE_TIME = 0x829A,
  ISO_SPEED = 0x8827,
  FOCAL_LENGTH = 0x920A,
  DATETIME_ORIGINAL = 0x9003,
  CREATE_DATETIME = 0x9004,
}


/**
 * Type definition of exif entry.
 */
export interface ExifEntry {
  id: ExifTag;
  format: number;
  componentCount: number;
  value?: any;
}
