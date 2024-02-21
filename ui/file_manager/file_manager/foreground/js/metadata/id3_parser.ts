// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ByteReader} from './byte_reader.js';
import {SeekOrigin} from './byte_reader.js';
import type {Id3v2Frame, ParserMetadata} from './metadata_item.js';
import {MetadataParser, type MetadataParserLogger} from './metadata_parser.js';

/**
 * ID3 parser.
 */
export class Id3Parser extends MetadataParser {
  /**
   * @param parent A metadata dispatcher.
   */
  constructor(parent: MetadataParserLogger) {
    super(parent, 'id3', /\.(mp3)$/i);
  }

  /**
   * Reads synchsafe integer.
   * 'SynchSafe' term is taken from id3 documentation.
   *
   * @param reader Reader to use.
   * @param length Rytes to read.
   * @return Synchsafe value.
   */
  private static readSynchSafe_(reader: ByteReader, length: number): number {
    let rv = 0;

    switch (length) {
      case 4:
        rv = reader.readScalar(1, false) << 21;
        // fall through
      case 3:
        rv |= reader.readScalar(1, false) << 14;
        // fall through
      case 2:
        rv |= reader.readScalar(1, false) << 7;
        // fall through
      case 1:
        rv |= reader.readScalar(1, false);
    }

    return rv;
  }

  /**
   * Reads 3bytes integer.
   *
   * @param reader Reader to use.
   * @return Uint24 value.
   */
  private static readUint24_(reader: ByteReader): number {
    return reader.readScalar(2, false) << 16 | reader.readScalar(1, false);
  }

  /**
   * Reads string from reader with specified encoding
   *
   * @param reader Reader to use.
   * @param encoding String encoding.
   * @param size Maximum string size. Actual result may be shorter.
   * @return String value.
   */
  private readString_(reader: ByteReader, encoding: number, size: number):
      string {
    switch (encoding) {
      case Id3Parser.V2.ENCODING.ISO_8859_1:
        return reader.readNullTerminatedString(size);

      case Id3Parser.V2.ENCODING.UTF_16:
        return reader.readNullTerminatedStringUtf16(true, size);

      case Id3Parser.V2.ENCODING.UTF_16BE:
        return reader.readNullTerminatedStringUtf16(false, size);

      case Id3Parser.V2.ENCODING.UTF_8:
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
   * @param reader Reader to use.
   * @param majorVersion Major id3 version to use.
   * @param frame Frame so store data at.
   * @param end Frame end position in reader.
   */
  private readTextFrame_(
      reader: ByteReader, _majorVersion: number, frame: Id3v2Frame,
      end: number) {
    frame.encoding = reader.readScalar(1, false, end);
    frame.value = this.readString_(reader, frame.encoding, end - reader.tell());
  }

  /**
   * Reads user defined text frame from reader.
   *
   * @param reader Reader to use.
   * @param majorVersion Major id3 version to use.
   * @param frame Frame so store data at.
   * @param end Frame end position in reader.
   */
  private readUserDefinedTextFrame_(
      reader: ByteReader, _majorVersion: number, frame: Id3v2Frame,
      end: number) {
    frame.encoding = reader.readScalar(1, false, end);

    frame.description =
        this.readString_(reader, frame.encoding, end - reader.tell());

    frame.value = this.readString_(reader, frame.encoding, end - reader.tell());
  }

  /**
   * @param reader Reader to use.
   * @param majorVersion Major id3 version to use.
   * @param frame Frame so store data at.
   * @param end Frame end position in reader.
   */
  private readPic_(
      reader: ByteReader, _majorVersion: number, frame: Id3v2Frame,
      end: number) {
    frame.encoding = reader.readScalar(1, false, end);
    frame.format = reader.readNullTerminatedString(3, end - reader.tell());
    frame.pictureType = reader.readScalar(1, false, end);
    frame.description =
        this.readString_(reader, frame.encoding, end - reader.tell());


    if (frame.format === '-->') {
      frame.imageUrl = reader.readNullTerminatedString(end - reader.tell());
    } else {
      frame.imageUrl = reader.readImage(end - reader.tell());
    }
  }

  /**
   * @param reader Reader to use.
   * @param majorVersion Major id3 version to use.
   * @param frame Frame so store data at.
   * @param end Frame end position in reader.
   */
  private readApic_(
      reader: ByteReader, _majorVersion: number, frame: Id3v2Frame,
      end: number) {
    this.vlog('Extracting picture');
    frame.encoding = reader.readScalar(1, false, end);
    frame.mime = reader.readNullTerminatedString(end - reader.tell());
    frame.pictureType = reader.readScalar(1, false, end);
    frame.description =
        this.readString_(reader, frame.encoding, end - reader.tell());

    if (frame.mime === '-->') {
      frame.imageUrl = reader.readNullTerminatedString(end - reader.tell());
    } else {
      frame.imageUrl = reader.readImage(end - reader.tell());
    }
  }

  /**
   * Reads string from reader with specified encoding
   *
   * @param reader Reader to use.
   * @param majorVersion Major id3 version to use.
   * @return Frame read.
   */
  private readFrame_(reader: ByteReader, majorVersion: number): Id3v2Frame
      |null {
    if (reader.eof()) {
      return null;
    }

    const frame: Id3v2Frame = {
      name: '',
      headerSize: 0,
      size: 0,
    };

    reader.pushSeek(reader.tell(), SeekOrigin.SEEK_BEG);

    const position = reader.tell();

    frame.name = (majorVersion === 2) ? reader.readNullTerminatedString(3) :
                                        reader.readNullTerminatedString(4);

    if (frame.name === '') {
      return null;
    }

    this.vlog('Found frame ' + (frame.name) + ' at position ' + position);

    switch (majorVersion) {
      case 2:
        frame.size = Id3Parser.readUint24_(reader);
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

    const handler = Id3Parser.V2.HANDLERS[frame.name];
    if (handler) {
      handler.call(
          this, reader, majorVersion, frame, reader.tell() + frame.size);
    } else if (frame.name.charAt(0) === 'T' || frame.name.charAt(0) === 'W') {
      this.readTextFrame_(
          reader, majorVersion, frame, reader.tell() + frame.size);
    }

    reader.popSeek();

    reader.seek(frame.size + frame.headerSize, SeekOrigin.SEEK_CUR);

    return frame;
  }

  /**
   * Parse the `file` and attempt to extract id3v1 metadata from it, and place
   * these properties on the `metadata` object.
   * @param file Input File object to parse.
   * @param metadata Output metadata object of the file.
   */
  private async parseId3v1(file: File, metadata: ParserMetadata) {
    // Reads last 128 bytes of file in bytebuffer, which passes further. In
    // last 128 bytes should be placed ID3v1 tag if available.
    const reader =
        await MetadataParser.readFileBytes(file, file.size - 128, file.size);

    // Attempts to extract ID3v1 tag from 128 bytes long ByteBuffer
    if (reader.readString(3) === 'TAG') {
      this.vlog('id3v1 found');
      const title = reader.readNullTerminatedString(30).trim();

      if (title.length > 0) {
        metadata.title = title;
      }

      reader.seek(3 + 30, SeekOrigin.SEEK_BEG);

      const artist = reader.readNullTerminatedString(30).trim();
      if (artist.length > 0) {
        metadata.artist = artist;
      }

      reader.seek(3 + 30 + 30, SeekOrigin.SEEK_BEG);

      const album = reader.readNullTerminatedString(30).trim();
      if (album.length > 0) {
        metadata.album = album;
      }
    }
  }

  /**
   * Parse the `file` and attempt to extract id3v2 metadata from it, and place
   * these properties on the `metadata` object.
   * @param file Input File object to parse.
   * @param metadata Output metadata object of the file.
   */
  private async parseId3v2(file: File, metadata: ParserMetadata) {
    let reader = await MetadataParser.readFileBytes(file, 0, 10);

    // Check if the first 10 bytes contains ID3 header.
    if (reader.readString(3) !== 'ID3') {
      return;
    }
    this.vlog('id3v2 found');
    const major = reader.readScalar(1, false);
    const minor = reader.readScalar(1, false);
    const flags = reader.readScalar(1, false);
    const size = Id3Parser.readSynchSafe_(reader, 4);
    const id3v2 = metadata.id3v2 = {
      majorVersion: major,
      minorVersion: minor,
      flags: flags,
      size: size,
      frames: {} as {[frameName: string]: Id3v2Frame},
    };

    // Extract all ID3v2 frames
    reader = await MetadataParser.readFileBytes(file, 10, 10 + id3v2.size);

    if ((id3v2.majorVersion > 2) &&
        ((id3v2.flags & Id3Parser.V2.FLAG_EXTENDED_HEADER) !== 0)) {
      // Skip extended header if found
      if (id3v2.majorVersion === 3) {
        reader.seek(reader.readScalar(4, false) - 4);
      } else if (id3v2.majorVersion === 4) {
        reader.seek(Id3Parser.readSynchSafe_(reader, 4) - 4);
      }
    }

    let frame;
    while (frame = this.readFrame_(reader, id3v2.majorVersion)) {
      id3v2.frames[frame.name] = frame;
    }
    if (id3v2.frames['APIC']) {
      metadata.thumbnailURL = id3v2.frames['APIC'].imageUrl;
    } else if (id3v2.frames['PIC']) {
      metadata.thumbnailURL = id3v2.frames['PIC'].imageUrl;
    }

    // Adds 'description' object to metadata. 'description' is used to unify
    // different parsers and make metadata parser-aware. The key of each
    // description item should be used to properly format the value before
    // displaying to users.
    metadata.description = [];

    for (const [key, frame] of Object.entries(id3v2.frames)) {
      const mappedKey = Id3Parser.V2.MAPPERS[key];
      if (mappedKey && frame.value && frame.value.trim().length > 0) {
        metadata.description.push({
          key: mappedKey,
          value: frame.value.trim(),
        });
      }
    }

    function extract(
        propName: 'album'|'title'|'artist', ...tagNames: string[]) {
      for (const tagName of tagNames) {
        const tag = id3v2.frames[tagName];
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
  }

  /**
   * @param file Input File object to parse.
   * @param metadata Output metadata object of the file.
   * @param callback Success callback.
   * @param onError Error callback.
   */
  parse(
      file: File, metadata: ParserMetadata,
      callback: (metadata: ParserMetadata) => void,
      onError: (error: string) => void) {
    this.log('Starting id3 parser for ' + file.name);

    Promise
        .all([this.parseId3v1(file, metadata), this.parseId3v2(file, metadata)])
        .then(() => {
          callback(metadata);
        })
        .catch((e: unknown) => {
          onError(e!.toString());
        });
  }

  /**
   * Metadata order to use for metadata generation
   */
  static readonly METADATA_ORDER = [
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
    'ID3_PUBLISHERS_OFFICIAL_WEBPAGE',
  ];


  /**
   * Id3v1 constants.
   */
  static readonly V1 = {
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
      'Synthpop',
    ],
  };


  /**
   * Id3v2 constants.
   */
  static readonly V2 = {
    FLAG_EXTENDED_HEADER: 1 << 5,

    ENCODING: {
      /**
       * ISO-8859-1 [ISO-8859-1]. Terminated with $00.
       */
      ISO_8859_1: 0,


      /**
       * [UTF-16] encoded Unicode [UNICODE] with BOM. All
       * strings in the same frame SHALL have the same byteorder.
       * Terminated with $00 00.
       */
      UTF_16: 1,

      /**
       * UTF-16BE [UTF-16] encoded Unicode [UNICODE] without BOM.
       * Terminated with $00 00.
       */
      UTF_16BE: 2,

      /**
       * UTF-8 [UTF-8] encoded Unicode [UNICODE]. Terminated with $00.
       */
      UTF_8: 3,
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
      PIC: Id3Parser.prototype.readPic_,

      // User attached image
      APIC: Id3Parser.prototype.readApic_,
    } as {[name: string]: Id3v2FrameHandler},
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
      WPUB: 'ID3_PUBLISHERS_OFFICIAL_WEBPAGE',
    } as {[name: string]: string},
  };
}

type Id3v2FrameHandler =
    (reader: ByteReader, majorVersion: number, frame: object, end: number) =>
        void;
