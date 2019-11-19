// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/function_sequence.js');
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/function_parallel.js');

/**
 * ID3 parser.
 * @final
 */
class Id3Parser extends MetadataParser {
  /**
   * @param {!MetadataParserLogger} parent A metadata dispatcher.
   */
  constructor(parent) {
    super(parent, 'id3', /\.(mp3)$/i);
  }

  /**
   * Reads synchsafe integer.
   * 'SynchSafe' term is taken from id3 documentation.
   *
   * @param {ByteReader} reader Reader to use.
   * @param {number} length Rytes to read.
   * @return {number} Synchsafe value.
   * @private
   */
  static readSynchSafe_(reader, length) {
    let rv = 0;

    switch (length) {
      case 4:
        rv = reader.readScalar(1, false) << 21;
      case 3:
        rv |= reader.readScalar(1, false) << 14;
      case 2:
        rv |= reader.readScalar(1, false) << 7;
      case 1:
        rv |= reader.readScalar(1, false);
    }

    return rv;
  }

  /**
   * Reads 3bytes integer.
   *
   * @param {ByteReader} reader Reader to use.
   * @return {number} Uint24 value.
   * @private
   */
  static readUInt24_(reader) {
    return reader.readScalar(2, false) << 16 | reader.readScalar(1, false);
  }

  /**
   * Reads string from reader with specified encoding
   *
   * @param {ByteReader} reader Reader to use.
   * @param {number} encoding String encoding.
   * @param {number} size Maximum string size. Actual result may be shorter.
   * @return {string} String value.
   * @private
   */
  readString_(reader, encoding, size) {
    switch (encoding) {
      case Id3Parser.v2.ENCODING.ISO_8859_1:
        return reader.readNullTerminatedString(size);

      case Id3Parser.v2.ENCODING.UTF_16:
        return reader.readNullTerminatedStringUTF16(true, size);

      case Id3Parser.v2.ENCODING.UTF_16BE:
        return reader.readNullTerminatedStringUTF16(false, size);

      case Id3Parser.v2.ENCODING.UTF_8:
        // TODO: implement UTF_8.
        this.log('UTF8 encoding not supported, used ISO_8859_1 instead');
        return reader.readNullTerminatedString(size);

      default: {
        this.log('Unsupported encoding in ID3 tag: ' + encoding);
        return '';
      }
    }
  }

  /**
   * Reads text frame from reader.
   *
   * @param {ByteReader} reader Reader to use.
   * @param {number} majorVersion Major id3 version to use.
   * @param {Object} frame Frame so store data at.
   * @param {number} end Frame end position in reader.
   * @private
   */
  readTextFrame_(reader, majorVersion, frame, end) {
    frame.encoding = reader.readScalar(1, false, end);
    frame.value = this.readString_(reader, frame.encoding, end - reader.tell());
  }

  /**
   * Reads user defined text frame from reader.
   *
   * @param {ByteReader} reader Reader to use.
   * @param {number} majorVersion Major id3 version to use.
   * @param {Object} frame Frame so store data at.
   * @param {number} end Frame end position in reader.
   * @private
   */
  readUserDefinedTextFrame_(reader, majorVersion, frame, end) {
    frame.encoding = reader.readScalar(1, false, end);

    frame.description =
        this.readString_(reader, frame.encoding, end - reader.tell());

    frame.value = this.readString_(reader, frame.encoding, end - reader.tell());
  }

  /**
   * @param {ByteReader} reader Reader to use.
   * @param {number} majorVersion Major id3 version to use.
   * @param {Object} frame Frame so store data at.
   * @param {number} end Frame end position in reader.
   * @private
   */
  readPIC_(reader, majorVersion, frame, end) {
    frame.encoding = reader.readScalar(1, false, end);
    frame.format = reader.readNullTerminatedString(3, end - reader.tell());
    frame.pictureType = reader.readScalar(1, false, end);
    frame.description =
        this.readString_(reader, frame.encoding, end - reader.tell());


    if (frame.format == '-->') {
      frame.imageUrl = reader.readNullTerminatedString(end - reader.tell());
    } else {
      frame.imageUrl = reader.readImage(end - reader.tell());
    }
  }

  /**
   * @param {ByteReader} reader Reader to use.
   * @param {number} majorVersion Major id3 version to use.
   * @param {Object} frame Frame so store data at.
   * @param {number} end Frame end position in reader.
   * @private
   */
  readAPIC_(reader, majorVersion, frame, end) {
    this.vlog('Extracting picture');
    frame.encoding = reader.readScalar(1, false, end);
    frame.mime = reader.readNullTerminatedString(end - reader.tell());
    frame.pictureType = reader.readScalar(1, false, end);
    frame.description =
        this.readString_(reader, frame.encoding, end - reader.tell());

    if (frame.mime == '-->') {
      frame.imageUrl = reader.readNullTerminatedString(end - reader.tell());
    } else {
      frame.imageUrl = reader.readImage(end - reader.tell());
    }
  }

  /**
   * Reads string from reader with specified encoding
   *
   * @param {ByteReader} reader Reader to use.
   * @param {number} majorVersion Major id3 version to use.
   * @return {Object} Frame read.
   * @private
   */
  readFrame_(reader, majorVersion) {
    if (reader.eof()) {
      return null;
    }

    const frame = {};

    reader.pushSeek(reader.tell(), ByteReader.SEEK_BEG);

    const position = reader.tell();

    frame.name = (majorVersion == 2) ? reader.readNullTerminatedString(3) :
                                       reader.readNullTerminatedString(4);

    if (frame.name == '') {
      return null;
    }

    this.vlog('Found frame ' + (frame.name) + ' at position ' + position);

    switch (majorVersion) {
      case 2:
        frame.size = Id3Parser.readUInt24_(reader);
        frame.headerSize = 6;
        break;
      case 3:
        frame.size = reader.readScalar(4, false);
        frame.headerSize = 10;
        frame.flags = reader.readScalar(2, false);
        break;
      case 4:
        frame.size = Id3Parser.readSynchSafe_(reader, 4);
        frame.headerSize = 10;
        frame.flags = reader.readScalar(2, false);
        break;
    }

    this.vlog(
        'Found frame [' + frame.name + '] with size [' + frame.size + ']');

    if (Id3Parser.v2.HANDLERS[frame.name]) {
      Id3Parser.v2.HANDLERS[frame.name].call(
          this, reader, majorVersion, frame, reader.tell() + frame.size);
    } else if (frame.name.charAt(0) == 'T' || frame.name.charAt(0) == 'W') {
      this.readTextFrame_(
          reader, majorVersion, frame, reader.tell() + frame.size);
    }

    reader.popSeek();

    reader.seek(frame.size + frame.headerSize, ByteReader.SEEK_CUR);

    return frame;
  }

  /**
   * @param {File} file File object to parse.
   * @param {Object} metadata Metadata object of the file.
   * @param {function(Object)} callback Success callback.
   * @param {function(string)} onError Error callback.
   */
  parse(file, metadata, callback, onError) {
    const self = this;

    this.log('Starting id3 parser for ' + file.name);

    const id3v1Parser = new FunctionSequence(
        'id3v1parser', [
          /**
           * Reads last 128 bytes of file in bytebuffer,
           * which passes further.
           * In last 128 bytes should be placed ID3v1 tag if available.
           * @param {File} file File which bytes to read.
           */
          function readTail(file) {
            MetadataParser.readFileBytes(
                file, file.size - 128, file.size, this.nextStep, this.onError);
          },

          /**
           * Attempts to extract ID3v1 tag from 128 bytes long ByteBuffer
           * @param {File} file File which tags are being extracted. Could be
           *     used for logging purposes.
           * @param {ByteReader} reader ByteReader of 128 bytes.
           */
          function extractId3v1(file, reader) {
            if (reader.readString(3) == 'TAG') {
              this.logger.vlog('id3v1 found');
              const id3v1 = metadata.id3v1 = {};

              const title = reader.readNullTerminatedString(30).trim();

              if (title.length > 0) {
                metadata.title = title;
              }

              reader.seek(3 + 30, ByteReader.SEEK_BEG);

              const artist = reader.readNullTerminatedString(30).trim();
              if (artist.length > 0) {
                metadata.artist = artist;
              }

              reader.seek(3 + 30 + 30, ByteReader.SEEK_BEG);

              const album = reader.readNullTerminatedString(30).trim();
              if (album.length > 0) {
                metadata.album = album;
              }
            }
            this.nextStep();
          }
        ],
        this, () => {}, error => {});

    const id3v2Parser = new FunctionSequence(
        'id3v2parser', [
          function readHead(file) {
            MetadataParser.readFileBytes(
                file, 0, 10, this.nextStep, this.onError);
          },

          /**
           * Check if passed array of 10 bytes contains ID3 header.
           * @param {File} file File to check and continue reading if ID3
           *     metadata found.
           * @param {ByteReader} reader Reader to fill with stream bytes.
           */
          function checkId3v2(file, reader) {
            if (reader.readString(3) == 'ID3') {
              this.logger.vlog('id3v2 found');
              const id3v2 = metadata.id3v2 = {};
              id3v2.major = reader.readScalar(1, false);
              id3v2.minor = reader.readScalar(1, false);
              id3v2.flags = reader.readScalar(1, false);
              id3v2.size = Id3Parser.readSynchSafe_(reader, 4);

              MetadataParser.readFileBytes(
                  file, 10, 10 + id3v2.size, this.nextStep, this.onError);
            } else {
              this.finish();
            }
          },

          /**
           * Extracts all ID3v2 frames from given bytebuffer.
           * @param {File} file File being parsed.
           * @param {ByteReader} reader Reader to use for metadata extraction.
           */
          function extractFrames(file, reader) {
            const id3v2 = metadata.id3v2;

            if ((id3v2.major > 2) &&
                (id3v2.flags & Id3Parser.v2.FLAG_EXTENDED_HEADER != 0)) {
              // Skip extended header if found
              if (id3v2.major == 3) {
                reader.seek(reader.readScalar(4, false) - 4);
              } else if (id3v2.major == 4) {
                reader.seek(Id3Parser.readSynchSafe_(reader, 4) - 4);
              }
            }

            let frame;

            while (frame = self.readFrame_(reader, id3v2.major)) {
              metadata.id3v2[frame.name] = frame;
            }

            this.nextStep();
          },

          /**
           * Adds 'description' object to metadata.
           * 'description' used to unify different parsers and make
           * metadata parser-aware.
           * Description is array if value-type pairs. Type should be used
           * to properly format value before displaying to user.
           */
          function prepareDescription() {
            const id3v2 = metadata.id3v2;

            if (id3v2['APIC']) {
              metadata.thumbnailURL = id3v2['APIC'].imageUrl;
            } else if (id3v2['PIC']) {
              metadata.thumbnailURL = id3v2['PIC'].imageUrl;
            }

            metadata.description = [];

            for (const key in id3v2) {
              if (typeof (Id3Parser.v2.MAPPERS[key]) != 'undefined' &&
                  id3v2[key].value.trim().length > 0) {
                metadata.description.push({
                  key: Id3Parser.v2.MAPPERS[key],
                  value: id3v2[key].value.trim()
                });
              }
            }

            /**
             * @param {string} propName
             * @param {...string} tags
             */
            function extract(propName, tags) {
              for (let i = 1; i != arguments.length; i++) {
                const tag = id3v2[arguments[i]];
                if (tag && tag.value) {
                  metadata[propName] = tag.value;
                  break;
                }
              }
            }

            extract('album', 'TALB', 'TAL');
            extract('title', 'TIT2', 'TT2');
            extract('artist', 'TPE1', 'TP1');

            metadata.description.sort((a, b) => {
              return Id3Parser.METADATA_ORDER.indexOf(a.key) -
                  Id3Parser.METADATA_ORDER.indexOf(b.key);
            });
            this.nextStep();
          }
        ],
        this, () => {}, error => {});

    const metadataParser = new FunctionParallel(
        'mp3metadataParser', [id3v1Parser, id3v2Parser], this, () => {
          callback.call(null, metadata);
        }, onError);

    id3v1Parser.setCallback(metadataParser.nextStep);
    id3v2Parser.setCallback(metadataParser.nextStep);

    id3v1Parser.setFailureCallback(metadataParser.onError);
    id3v2Parser.setFailureCallback(metadataParser.onError);

    this.vlog('Passed argument : ' + file);

    metadataParser.start(file);
  }
}

/**
 * Metadata order to use for metadata generation
 * @type {Array<string>}
 * @const
 */
Id3Parser.METADATA_ORDER = [
  'ID3_TITLE',
  'ID3_LEAD_PERFORMER',
  'ID3_YEAR',
  'ID3_ALBUM',
  'ID3_TRACK_NUMBER',
  'ID3_BPM',
  'ID3_COMPOSER',
  'ID3_DATE',
  'ID3_PLAYLIST_DELAY',
  'ID3_LYRICIST',
  'ID3_FILE_TYPE',
  'ID3_TIME',
  'ID3_LENGTH',
  'ID3_FILE_OWNER',
  'ID3_BAND',
  'ID3_COPYRIGHT',
  'ID3_OFFICIAL_AUDIO_FILE_WEBPAGE',
  'ID3_OFFICIAL_ARTIST',
  'ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE',
  'ID3_PUBLISHERS_OFFICIAL_WEBPAGE'
];


/**
 * Id3v1 constants.
 * @type {Object<*>}
 */
Id3Parser.v1 = {
  /**
   * Genres list as described in id3 documentation. We aren't going to
   * localize this list, because at least in Russian (and I think most
   * other languages), translation exists at least for 10% and most time
   * translation would degrade to transliteration.
   */
  GENRES: [
    'Blues',
    'Classic Rock',
    'Country',
    'Dance',
    'Disco',
    'Funk',
    'Grunge',
    'Hip-Hop',
    'Jazz',
    'Metal',
    'New Age',
    'Oldies',
    'Other',
    'Pop',
    'R&B',
    'Rap',
    'Reggae',
    'Rock',
    'Techno',
    'Industrial',
    'Alternative',
    'Ska',
    'Death Metal',
    'Pranks',
    'Soundtrack',
    'Euro-Techno',
    'Ambient',
    'Trip-Hop',
    'Vocal',
    'Jazz+Funk',
    'Fusion',
    'Trance',
    'Classical',
    'Instrumental',
    'Acid',
    'House',
    'Game',
    'Sound Clip',
    'Gospel',
    'Noise',
    'AlternRock',
    'Bass',
    'Soul',
    'Punk',
    'Space',
    'Meditative',
    'Instrumental Pop',
    'Instrumental Rock',
    'Ethnic',
    'Gothic',
    'Darkwave',
    'Techno-Industrial',
    'Electronic',
    'Pop-Folk',
    'Eurodance',
    'Dream',
    'Southern Rock',
    'Comedy',
    'Cult',
    'Gangsta',
    'Top 40',
    'Christian Rap',
    'Pop/Funk',
    'Jungle',
    'Native American',
    'Cabaret',
    'New Wave',
    'Psychadelic',
    'Rave',
    'Showtunes',
    'Trailer',
    'Lo-Fi',
    'Tribal',
    'Acid Punk',
    'Acid Jazz',
    'Polka',
    'Retro',
    'Musical',
    'Rock & Roll',
    'Hard Rock',
    'Folk',
    'Folk-Rock',
    'National Folk',
    'Swing',
    'Fast Fusion',
    'Bebob',
    'Latin',
    'Revival',
    'Celtic',
    'Bluegrass',
    'Avantgarde',
    'Gothic Rock',
    'Progressive Rock',
    'Psychedelic Rock',
    'Symphonic Rock',
    'Slow Rock',
    'Big Band',
    'Chorus',
    'Easy Listening',
    'Acoustic',
    'Humour',
    'Speech',
    'Chanson',
    'Opera',
    'Chamber Music',
    'Sonata',
    'Symphony',
    'Booty Bass',
    'Primus',
    'Porn Groove',
    'Satire',
    'Slow Jam',
    'Club',
    'Tango',
    'Samba',
    'Folklore',
    'Ballad',
    'Power Ballad',
    'Rhythmic Soul',
    'Freestyle',
    'Duet',
    'Punk Rock',
    'Drum Solo',
    'A capella',
    'Euro-House',
    'Dance Hall',
    'Goa',
    'Drum & Bass',
    'Club-House',
    'Hardcore',
    'Terror',
    'Indie',
    'BritPop',
    'Negerpunk',
    'Polsk Punk',
    'Beat',
    'Christian Gangsta Rap',
    'Heavy Metal',
    'Black Metal',
    'Crossover',
    'Contemporary Christian',
    'Christian Rock',
    'Merengue',
    'Salsa',
    'Thrash Metal',
    'Anime',
    'Jpop',
    'Synthpop'
  ]
};

/**
 * Id3v2 constants.
 * @type {Object<*>}
 */
Id3Parser.v2 = {
  FLAG_EXTENDED_HEADER: 1 << 5,

  ENCODING: {
    /**
     * ISO-8859-1 [ISO-8859-1]. Terminated with $00.
     *
     * @const
     * @type {number}
     */
    ISO_8859_1: 0,


    /**
     * [UTF-16] encoded Unicode [UNICODE] with BOM. All
     * strings in the same frame SHALL have the same byteorder.
     * Terminated with $00 00.
     *
     * @const
     * @type {number}
     */
    UTF_16: 1,

    /**
     * UTF-16BE [UTF-16] encoded Unicode [UNICODE] without BOM.
     * Terminated with $00 00.
     *
     * @const
     * @type {number}
     */
    UTF_16BE: 2,

    /**
     * UTF-8 [UTF-8] encoded Unicode [UNICODE]. Terminated with $00.
     *
     * @const
     * @type {number}
     */
    UTF_8: 3
  },
  HANDLERS: {
    // User defined text information frame
    TXX: Id3Parser.prototype.readUserDefinedTextFrame_,
    // User defined URL link frame
    WXX: Id3Parser.prototype.readUserDefinedTextFrame_,

    // User defined text information frame
    TXXX: Id3Parser.prototype.readUserDefinedTextFrame_,

    // User defined URL link frame
    WXXX: Id3Parser.prototype.readUserDefinedTextFrame_,

    // User attached image
    PIC: Id3Parser.prototype.readPIC_,

    // User attached image
    APIC: Id3Parser.prototype.readAPIC_
  },
  MAPPERS: {
    TALB: 'ID3_ALBUM',
    TBPM: 'ID3_BPM',
    TCOM: 'ID3_COMPOSER',
    TDAT: 'ID3_DATE',
    TDLY: 'ID3_PLAYLIST_DELAY',
    TEXT: 'ID3_LYRICIST',
    TFLT: 'ID3_FILE_TYPE',
    TIME: 'ID3_TIME',
    TIT2: 'ID3_TITLE',
    TLEN: 'ID3_LENGTH',
    TOWN: 'ID3_FILE_OWNER',
    TPE1: 'ID3_LEAD_PERFORMER',
    TPE2: 'ID3_BAND',
    TRCK: 'ID3_TRACK_NUMBER',
    TYER: 'ID3_YEAR',
    WCOP: 'ID3_COPYRIGHT',
    WOAF: 'ID3_OFFICIAL_AUDIO_FILE_WEBPAGE',
    WOAR: 'ID3_OFFICIAL_ARTIST',
    WOAS: 'ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE',
    WPUB: 'ID3_PUBLISHERS_OFFICIAL_WEBPAGE'
  }
};
