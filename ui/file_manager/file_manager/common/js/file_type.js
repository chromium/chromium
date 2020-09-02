// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace object for file type utility functions.
 */
function FileType() {}

/**
 * @typedef {{
 *   name: !string,
 *   type: !string,
 *   icon: !string,
 *   subtype: !string,
 *   pattern: (RegExp|undefined),
 *   mimePattern: (RegExp|undefined)
 * }}
 */
FileType.Descriptor;

/**
 * Description of known file types.
 * Pair type-subtype defines order when sorted by file type.
 * @type {Array<!FileType.Descriptor>}
 */
FileType.types = [
  // Images
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'JPEG',
    pattern: /\.jpe?g$/i,
    mimePattern: /image\/jpeg/i,
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'BMP',
    pattern: /\.bmp$/i,
    mimePattern: /image\/bmp/i,
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'GIF',
    pattern: /\.gif$/i,
    mimePattern: /image\/gif/i,
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'ICO',
    pattern: /\.ico$/i,
    mimePattern: /image\/x\-icon/i,
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'PNG',
    pattern: /\.png$/i,
    mimePattern: /image\/png/i
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'WebP',
    pattern: /\.webp$/i,
    mimePattern: /image\/webp/i
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'TIFF',
    pattern: /\.tiff?$/i,
    mimePattern: /image\/tiff/i
  },
  {
    type: 'image',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'SVG',
    pattern: /\.svg$/i,
    mimePattern: /image\/svg\+xml/i
  },

  // Raw
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'ARW',
    pattern: /\.arw$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'CR2',
    pattern: /\.cr2$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'DNG',
    pattern: /\.dng$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'NEF',
    pattern: /\.nef$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'NRW',
    pattern: /\.nrw$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'ORF',
    pattern: /\.orf$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'RAF',
    pattern: /\.raf$/i,
    icon: 'image'
  },
  {
    type: 'raw',
    name: 'IMAGE_FILE_TYPE',
    subtype: 'RW2',
    pattern: /\.rw2$/i,
    icon: 'image'
  },

  // Video
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: '3GP',
    pattern: /\.3gpp?$/i,
    mimePattern: /video\/3gpp/i
  },
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: 'AVI',
    pattern: /\.avi$/i,
    mimePattern: /video\/x\-msvideo/i
  },
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: 'QuickTime',
    pattern: /\.mov$/i,
    mimePattern: /video\/quicktime/i
  },
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: 'MKV',
    pattern: /\.mkv$/i,
    mimePattern: /video\/x\-matroska/i
  },
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: 'MPEG',
    pattern: /\.m(p4|4v|pg|peg|pg4|peg4)$/i,
    mimePattern: /video\/mp(4|eg)/i
  },
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: 'OGG',
    pattern: /\.og(m|v|x)$/i,
    mimePattern: /(application|video)\/ogg/i
  },
  {
    type: 'video',
    name: 'VIDEO_FILE_TYPE',
    subtype: 'WebM',
    pattern: /\.webm$/i,
    mimePattern: /video\/webm/i
  },

  // Audio
  {
    type: 'audio',
    name: 'AUDIO_FILE_TYPE',
    subtype: 'AMR',
    pattern: /\.amr$/i,
    mimePattern: /audio\/amr/i
  },
  {
    type: 'audio',
    name: 'AUDIO_FILE_TYPE',
    subtype: 'FLAC',
    pattern: /\.flac$/i,
    mimePattern: /audio\/flac/i
  },
  {
    type: 'audio',
    name: 'AUDIO_FILE_TYPE',
    subtype: 'MP3',
    pattern: /\.mp3$/i,
    mimePattern: /audio\/mpeg/i
  },
  {
    type: 'audio',
    name: 'AUDIO_FILE_TYPE',
    subtype: 'MPEG',
    pattern: /\.m4a$/i,
    mimePattern: /audio\/mp4a-latm/i
  },
  {
    type: 'audio',
    name: 'AUDIO_FILE_TYPE',
    subtype: 'OGG',
    pattern: /\.o(g(a|g)|pus)$/i,
    mimePattern: /audio\/ogg/i
  },
  {
    type: 'audio',
    name: 'AUDIO_FILE_TYPE',
    subtype: 'WAV',
    pattern: /\.wav$/i,
    mimePattern: /audio\/x\-wav/i
  },

  // Text
  {
    type: 'text',
    name: 'PLAIN_TEXT_FILE_TYPE',
    subtype: 'TXT',
    pattern: /\.txt$/i,
    mimePattern: /text\/plain/i
  },

  // Archive
  {
    type: 'archive',
    name: 'ZIP_ARCHIVE_FILE_TYPE',
    subtype: 'ZIP',
    pattern: /\.zip$/i,
    mimePattern: /application\/zip/i
  },
  {
    type: 'archive',
    name: 'RAR_ARCHIVE_FILE_TYPE',
    subtype: 'RAR',
    pattern: /\.rar$/i,
    mimePattern: /application\/x\-rar\-compressed/i
  },
  {
    type: 'archive',
    name: 'TAR_ARCHIVE_FILE_TYPE',
    subtype: 'TAR',
    pattern: /\.tar$/i,
    mimePattern: /application\/x\-tar/i
  },
  {
    type: 'archive',
    name: 'TAR_BZIP2_ARCHIVE_FILE_TYPE',
    subtype: 'TBZ2',
    pattern: /\.(tar\.bz2|tbz|tbz2)$/i,
    mimePattern: /application\/x\-bzip2/i
  },
  {
    type: 'archive',
    name: 'TAR_GZIP_ARCHIVE_FILE_TYPE',
    subtype: 'TGZ',
    pattern: /\.(tar\.|t)gz$/i,
    mimePattern: /application\/x\-gzip/i
  },

  // Hosted docs.
  {
    type: 'hosted',
    icon: 'gdoc',
    name: 'GDOC_DOCUMENT_FILE_TYPE',
    subtype: 'doc',
    pattern: /\.gdoc$/i
  },
  {
    type: 'hosted',
    icon: 'gsheet',
    name: 'GSHEET_DOCUMENT_FILE_TYPE',
    subtype: 'sheet',
    pattern: /\.gsheet$/i
  },
  {
    type: 'hosted',
    icon: 'gslides',
    name: 'GSLIDES_DOCUMENT_FILE_TYPE',
    subtype: 'slides',
    pattern: /\.gslides$/i
  },
  {
    type: 'hosted',
    icon: 'gdraw',
    name: 'GDRAW_DOCUMENT_FILE_TYPE',
    subtype: 'draw',
    pattern: /\.gdraw$/i
  },
  {
    type: 'hosted',
    icon: 'gtable',
    name: 'GTABLE_DOCUMENT_FILE_TYPE',
    subtype: 'table',
    pattern: /\.gtable$/i
  },
  {
    type: 'hosted',
    icon: 'glink',
    name: 'GLINK_DOCUMENT_FILE_TYPE',
    subtype: 'glink',
    pattern: /\.glink$/i
  },
  {
    type: 'hosted',
    icon: 'gform',
    name: 'GFORM_DOCUMENT_FILE_TYPE',
    subtype: 'form',
    pattern: /\.gform$/i
  },
  {
    type: 'hosted',
    icon: 'gmap',
    name: 'GMAP_DOCUMENT_FILE_TYPE',
    subtype: 'map',
    pattern: /\.gmap$/i
  },
  {
    type: 'hosted',
    icon: 'gsite',
    name: 'GSITE_DOCUMENT_FILE_TYPE',
    subtype: 'site',
    pattern: /\.gsite$/i
  },

  // Others
  {
    type: 'document',
    icon: 'pdf',
    name: 'PDF_DOCUMENT_FILE_TYPE',
    subtype: 'PDF',
    pattern: /\.pdf$/i,
    mimePattern: /application\/pdf/i
  },
  {
    type: 'document',
    name: 'HTML_DOCUMENT_FILE_TYPE',
    subtype: 'HTML',
    pattern: /\.(html?|mht(ml)?|shtml|xht(ml)?)$/i,
    mimePattern: /text\/html/i
  },
  {
    type: 'document',
    icon: 'word',
    name: 'WORD_DOCUMENT_FILE_TYPE',
    subtype: 'Word',
    pattern: /\.(doc|docx)$/i,
    mimePattern: new RegExp(
        'application/(msword|vnd\\.' +
            'openxmlformats-officedocument\\.wordprocessingml\\.document)',
        'i')
  },
  {
    type: 'document',
    icon: 'ppt',
    name: 'POWERPOINT_PRESENTATION_FILE_TYPE',
    subtype: 'PPT',
    pattern: /\.(ppt|pptx)$/i,
    mimePattern: new RegExp(
        'application/vnd\\.(ms-powerpoint|' +
            'openxmlformats-officedocument\\.presentationml\\.presentation)',
        'i')
  },
  {
    type: 'document',
    icon: 'excel',
    name: 'EXCEL_FILE_TYPE',
    subtype: 'Excel',
    pattern: /\.(xls|xlsx)$/i,
    mimePattern: new RegExp(
        'application/vnd\\.(ms-excel|' +
            'openxmlformats-officedocument\\.spreadsheetml\\.sheet)',
        'i')
  },
  {
    type: 'archive',
    icon: 'tini',
    name: 'TINI_FILE_TYPE',
    subtype: 'TGZ',
    pattern: /\.tini$/i,
  }
];

/**
 * A special type for directory.
 * @type{!FileType.Descriptor}
 * @const
 */
FileType.DIRECTORY = {
  name: 'FOLDER',
  type: '.folder',
  icon: 'folder',
  subtype: ''
};

/**
 * A special placeholder for unknown types with no extension.
 * @type{!FileType.Descriptor}
 * @const
 */
FileType.PLACEHOLDER = {
  name: 'NO_EXTENSION_FILE_TYPE',
  type: 'UNKNOWN',
  icon: '',
  subtype: ''
};

/**
 * Returns the file path extension for a given file.
 *
 * @param {Entry|FilesAppEntry} entry Reference to the file.
 * @return {string} The extension including a leading '.', or empty string if
 *     not found.
 */
FileType.getExtension = entry => {
  // No extension for a directory.
  if (entry.isDirectory) {
    return '';
  }

  const extensionStartIndex = entry.name.lastIndexOf('.');
  if (extensionStartIndex === -1 ||
      extensionStartIndex === entry.name.length - 1) {
    return '';
  }

  return entry.name.substr(extensionStartIndex);
};

/**
 * Gets the file type object for a given file name (base name). Use getType()
 * if possible, since this method can't recognize directories.
 *
 * @param {string} name Name of the file.
 * @return {!FileType.Descriptor} The matching descriptor or a placeholder.
 */
FileType.getTypeForName = name => {
  const types = FileType.types;
  for (let i = 0; i < types.length; i++) {
    if (types[i].pattern.test(name)) {
      return types[i];
    }
  }

  // Unknown file type.
  const match = /\.[^\/\.]+$/.exec(name);
  const extension = match ? match[0] : '';
  if (extension === '') {
    return FileType.PLACEHOLDER;
  }

  // subtype is the extension excluding the first dot.
  return {
    name: 'GENERIC_FILE_TYPE',
    type: 'UNKNOWN',
    subtype: extension.substr(1).toUpperCase(),
    icon: ''
  };
};

/**
 * Gets the file type object for a given entry. If mime type is provided, then
 * uses it with higher priority than the extension.
 *
 * @param {(Entry|FilesAppEntry)} entry Reference to the entry.
 * @param {string=} opt_mimeType Optional mime type for the entry.
 * @return {!FileType.Descriptor} The matching descriptor or a placeholder.
 */
FileType.getType = (entry, opt_mimeType) => {
  if (entry.isDirectory) {
    // For removable partitions, use the file system type.
    if (/** @type {VolumeEntry}*/ (entry).volumeInfo &&
        /** @type {VolumeEntry}*/ (entry).volumeInfo.diskFileSystemType) {
      return {
        name: '',
        type: 'partition',
        subtype:
            /** @type {VolumeEntry}*/ (entry).volumeInfo.diskFileSystemType,
        icon: '',
      };
    }
    return FileType.DIRECTORY;
  }

  if (opt_mimeType) {
    for (let i = 0; i < FileType.types.length; i++) {
      if (FileType.types[i].mimePattern &&
          FileType.types[i].mimePattern.test(opt_mimeType)) {
        return FileType.types[i];
      }
    }
  }

  for (let i = 0; i < FileType.types.length; i++) {
    if (FileType.types[i].pattern.test(entry.name)) {
      return FileType.types[i];
    }
  }

  // Unknown file type.
  const extension = FileType.getExtension(entry);
  if (extension === '') {
    return FileType.PLACEHOLDER;
  }

  // subtype is the extension excluding the first dot.
  return {
    name: 'GENERIC_FILE_TYPE',
    type: 'UNKNOWN',
    subtype: extension.substr(1).toUpperCase(),
    icon: ''
  };
};

/**
 * Gets the media type for a given file.
 *
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {string} The value of 'type' property from one of the elements in
 *     FileType.types or undefined.
 */
FileType.getMediaType = (entry, opt_mimeType) => {
  return FileType.getType(entry, opt_mimeType).type;
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if audio file.
 */
FileType.isAudio = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'audio';
};

/**
 * Returns whether the |entry| is image file that can be opened in browser.
 * Note that it returns false for RAW images.
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if image file.
 */
FileType.isImage = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'image';
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if video file.
 */
FileType.isVideo = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'video';
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if raw file.
 */
FileType.isRaw = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'raw';
};

/**
 * @param {Entry} entry Reference to the file
 * @param {string=} opt_mimeType Optional mime type for this file.
 * @return {boolean} Whether or not this is a PDF file.
 */
FileType.isPDF = (entry, opt_mimeType) => {
  return FileType.getType(entry, opt_mimeType).subtype === 'PDF';
};

/**
 * Files with more pixels won't have preview.
 * @param {!Array<string>} types
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if type is in specified set
 */
FileType.isType = (types, entry, opt_mimeType) => {
  const type = FileType.getMediaType(entry, opt_mimeType);
  return !!type && types.indexOf(type) !== -1;
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} Returns true if the file is hosted.
 */
FileType.isHosted = (entry, opt_mimeType) => {
  return FileType.getType(entry, opt_mimeType).type === 'hosted';
};

/**
 * @param {Entry|VolumeEntry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @param {VolumeManagerCommon.RootType=} opt_rootType The root type of the
 *     entry.
 * @return {string} Returns string that represents the file icon.
 *     It refers to a file 'images/filetype_' + icon + '.png'.
 */
FileType.getIcon = (entry, opt_mimeType, opt_rootType) => {
  const fileType = FileType.getType(entry, opt_mimeType);
  const overridenIcon = FileType.getIconOverrides(entry, opt_rootType);
  return entry.iconName || overridenIcon || fileType.icon || fileType.type ||
      'unknown';
};

/**
 * Returns a string to be used as an attribute value to customize the entry
 * icon.
 *
 * @param {Entry|FilesAppEntry} entry
 * @param {VolumeManagerCommon.RootType=} opt_rootType The root type of the
 *     entry.
 * @return {string}
 */
FileType.getIconOverrides = (entry, opt_rootType) => {
  // Overrides per RootType and defined by fullPath.
  const overrides = {
    [VolumeManagerCommon.RootType.DOWNLOADS]: {
      '/Downloads': VolumeManagerCommon.VolumeType.DOWNLOADS,
      '/PvmDefault': 'plugin_vm',
    },
  };
  const root = overrides[opt_rootType];
  return root ? root[entry.fullPath] : '';
};
