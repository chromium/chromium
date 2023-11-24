// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ByteReader, SeekOrigin} from './byte_reader.js';
import {MetadataParser} from './metadata_parser.js';

/** @final */
export class MpegParser extends MetadataParser {
  /**
   * @param {!import("./metadata_parser.js").MetadataParserLogger}
   *     parent Parent object.
   */
  constructor(parent) {
    super(parent, 'mpeg', /\.(mp4|m4v|m4a|mpe?g4?)$/i);
    this.mimeType = 'video/mpeg';
  }

  /**
   * @param {ByteReader} br ByteReader instance.
   * @param {number=} opt_end End of atom position.
   * @return {number} Atom size.
   */
  static readAtomSize(br, opt_end) {
    const pos = br.tell();

    if (opt_end) {
      // Assert that opt_end <= buffer end.
      // When supplied, opt_end is the end of the enclosing atom and is used to
      // check the correct nesting.
      br.validateRead(opt_end - pos);
    }

    const size = br.readScalar(4, false, opt_end);

    if (size < MpegParser.HEADER_SIZE) {
      throw new Error('atom too short (' + size + ') @' + pos);
    }

    if (opt_end && pos + size > opt_end) {
      throw new Error(
          'atom too long (' + size + '>' + (opt_end - pos) + ') @' + pos);
    }

    return size;
  }

  /**
   * @param {ByteReader} br ByteReader instance.
   * @param {number=} opt_end End of atom position.
   * @return {string} Atom name.
   */
  static readAtomName(br, opt_end) {
    return br.readString(4, opt_end).toLowerCase();
  }

  /**
   * @param {Object} metadata Metadata object.
   * @return {Object} Root of the parser tree.
   */
  static createRootParser(metadata) {
    // @ts-ignore: error TS7006: Parameter 'name' implicitly has an 'any' type.
    function findParentAtom(atom, name) {
      for (;;) {
        atom = atom.parent;
        if (!atom) {
          return null;
        }
        if (atom.name == name) {
          return atom;
        }
      }
    }

    // @ts-ignore: error TS7006: Parameter 'atom' implicitly has an 'any' type.
    function parseFtyp(br, atom) {
      // @ts-ignore: error TS2339: Property 'brand' does not exist on type
      // 'Object'.
      metadata.brand = br.readString(4, atom.end);
    }

    // @ts-ignore: error TS7006: Parameter 'atom' implicitly has an 'any' type.
    function parseMvhd(br, atom) {
      const version = br.readScalar(4, false, atom.end);
      const offset = (version == 0) ? 8 : 16;
      br.seek(offset, SeekOrigin.SEEK_CUR);
      const timescale = br.readScalar(4, false, atom.end);
      const duration = br.readScalar(4, false, atom.end);
      // @ts-ignore: error TS2339: Property 'duration' does not exist on type
      // 'Object'.
      metadata.duration = duration / timescale;
    }

    // @ts-ignore: error TS7006: Parameter 'atom' implicitly has an 'any' type.
    function parseHdlr(br, atom) {
      br.seek(8, SeekOrigin.SEEK_CUR);
      findParentAtom(atom, 'trak').trackType = br.readString(4, atom.end);
    }

    // @ts-ignore: error TS7006: Parameter 'atom' implicitly has an 'any' type.
    function parseStsd(br, atom) {
      const track = findParentAtom(atom, 'trak');
      if (track && track.trackType == 'vide') {
        br.seek(40, SeekOrigin.SEEK_CUR);
        // @ts-ignore: error TS2339: Property 'width' does not exist on type
        // 'Object'.
        metadata.width = br.readScalar(2, false, atom.end);
        // @ts-ignore: error TS2339: Property 'height' does not exist on type
        // 'Object'.
        metadata.height = br.readScalar(2, false, atom.end);
      }
    }

    // @ts-ignore: error TS7006: Parameter 'atom' implicitly has an 'any' type.
    function parseDataString(name, br, atom) {
      br.seek(8, SeekOrigin.SEEK_CUR);
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'any' can't be used to index type 'Object'.
      metadata[name] = br.readString(atom.end - br.tell(), atom.end);
    }

    // @ts-ignore: error TS7006: Parameter 'atom' implicitly has an 'any' type.
    function parseCovr(br, atom) {
      br.seek(8, SeekOrigin.SEEK_CUR);
      // @ts-ignore: error TS2339: Property 'thumbnailURL' does not exist on
      // type 'Object'.
      metadata.thumbnailURL = br.readImage(atom.end - br.tell(), atom.end);
    }

    // 'meta' atom can occur at one of the several places in the file structure.
    const parseMeta = {
      ilst: {
        '©nam': {data: parseDataString.bind(null, 'title')},
        '©alb': {data: parseDataString.bind(null, 'album')},
        '©art': {data: parseDataString.bind(null, 'artist')},
        'covr': {data: parseCovr},
      },
      versioned: true,
    };

    // main parser for the entire file structure.
    return {
      ftyp: parseFtyp,
      moov: {
        mvhd: parseMvhd,
        trak: {
          mdia: {
            hdlr: parseHdlr,
            minf: {
              stbl: {stsd: parseStsd},
            },
          },
          meta: parseMeta,
        },
        udta: {
          meta: parseMeta,
        },
        meta: parseMeta,
      },
      meta: parseMeta,
    };
  }

  /**
   * @param {File} file File.
   * @param {Object} metadata Metadata.
   * @param {function(Object):void} callback Success callback.
   * @param {function((ProgressEvent|string)):void} onError Error callback.
   */
  parse(file, metadata, callback, onError) {
    const rootParser = MpegParser.createRootParser(metadata);

    // Kick off the processing by reading the first atom's header.
    this.requestRead(
        rootParser, file, 0, MpegParser.HEADER_SIZE, null, onError,
        callback.bind(null, metadata));
  }

  /**
   * @param {function(ByteReader, Object):void|Object} parser Parser tree node.
   * @param {ByteReader} br ByteReader instance.
   * @param {Object} atom Atom descriptor.
   * @param {number} filePos File position of the atom start.
   */
  applyParser(parser, br, atom, filePos) {
    if (this.verbose) {
      // @ts-ignore: error TS2339: Property 'name' does not exist on type
      // 'Object'.
      let path = atom.name;
      // @ts-ignore: error TS2339: Property 'parent' does not exist on type
      // 'Object'.
      for (let p = atom.parent; p && p.name; p = p.parent) {
        path = p.name + '.' + path;
      }

      let action;
      if (!parser) {
        action = 'skipping ';
      } else if (parser instanceof Function) {
        action = 'parsing  ';
      } else {
        action = 'recursing';
      }

      // @ts-ignore: error TS2339: Property 'start' does not exist on type
      // 'Object'.
      const start = atom.start - MpegParser.HEADER_SIZE;
      this.vlog(
          path + ': ' +
              // @ts-ignore: error TS2339: Property 'end' does not exist on type
              // 'Object'.
              '@' + (filePos + start) + ':' + (atom.end - start),
          action);
    }

    if (parser) {
      if (parser instanceof Function) {
        // @ts-ignore: error TS2339: Property 'start' does not exist on type
        // 'Object'.
        br.pushSeek(atom.start);
        parser(br, atom);
        br.popSeek();
      } else {
        // @ts-ignore: error TS2339: Property 'versioned' does not exist on type
        // 'Object'.
        if (parser.versioned) {
          // @ts-ignore: error TS2339: Property 'start' does not exist on type
          // 'Object'.
          atom.start += 4;
        }
        this.parseMpegAtomsInRange(parser, br, atom, filePos);
      }
    }
  }

  /**
   * @param {function(ByteReader, Object):void|Object} parser Parser tree node.
   * @param {ByteReader} br ByteReader instance.
   * @param {Object} parentAtom Parent atom descriptor.
   * @param {number} filePos File position of the atom start.
   */
  parseMpegAtomsInRange(parser, br, parentAtom, filePos) {
    let count = 0;
    // @ts-ignore: error TS2339: Property 'end' does not exist on type 'Object'.
    for (let offset = parentAtom.start; offset != parentAtom.end;) {
      if (count++ > 100) {
        // Most likely we are looping through a corrupt file.
        throw new Error(
            // @ts-ignore: error TS2339: Property 'name' does not exist on type
            // 'Object'.
            'too many child atoms in ' + parentAtom.name + ' @' + offset);
      }

      br.seek(offset);
      // @ts-ignore: error TS2339: Property 'end' does not exist on type
      // 'Object'.
      const size = MpegParser.readAtomSize(br, parentAtom.end);
      // @ts-ignore: error TS2339: Property 'end' does not exist on type
      // 'Object'.
      const name = MpegParser.readAtomName(br, parentAtom.end);

      this.applyParser(
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'string' can't be used to index type
          // 'Object | ((arg0: ByteReader, arg1: Object) => any)'.
          parser[name], br, {
            start: offset + MpegParser.HEADER_SIZE,
            end: offset + size,
            name: name,
            parent: parentAtom,
          },
          filePos);

      offset += size;
    }
  }

  /**
   * @param {Object} rootParser Parser definition.
   * @param {File} file File.
   * @param {number} filePos Start position in the file.
   * @param {number} size Atom size.
   * @param {string?} name Atom name.
   * @param {function((ProgressEvent|string)):void} onError Error callback.
   * @param {function():void} onSuccess Success callback.
   */
  requestRead(rootParser, file, filePos, size, name, onError, onSuccess) {
    const self = this;
    const reader = new FileReader();
    reader.onerror = onError;
    // @ts-ignore: error TS6133: 'event' is declared but its value is never
    // read.
    reader.onload = event => {
      self.processTopLevelAtom(
          /** @type {ArrayBuffer} */ (reader.result), rootParser, file, filePos,
          size, name, onError, onSuccess);
    };
    this.vlog('reading @' + filePos + ':' + size);
    reader.readAsArrayBuffer(file.slice(filePos, filePos + size));
  }

  /**
   * @param {ArrayBuffer} buf Data buffer.
   * @param {Object} rootParser Parser definition.
   * @param {File} file File.
   * @param {number} filePos Start position in the file.
   * @param {number} size Atom size.
   * @param {string?} name Atom name.
   * @param {function((ProgressEvent|string)):void} onError Error callback.
   * @param {function():void} onSuccess Success callback.
   */
  processTopLevelAtom(
      buf, rootParser, file, filePos, size, name, onError, onSuccess) {
    try {
      const br = new ByteReader(buf);

      // the header has already been read.
      const atomEnd = size - MpegParser.HEADER_SIZE;

      const bufLength = buf.byteLength;

      // Check the available data size. It should be either exactly
      // what we requested or HEADER_SIZE bytes less (for the last atom).
      if (bufLength != atomEnd && bufLength != size) {
        throw new Error(
            'Read failure @' + filePos + ', ' +
            'requested ' + size + ', read ' + bufLength);
      }

      // Process the top level atom.
      if (name) {  // name is null only the first time.
        this.applyParser(
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // 'Object'.
            rootParser[name], br, {start: 0, end: atomEnd, name: name},
            filePos);
      }

      filePos += bufLength;
      if (bufLength == size) {
        // The previous read returned everything we asked for, including
        // the next atom header at the end of the buffer.
        // Parse this header and schedule the next read.
        br.seek(-MpegParser.HEADER_SIZE, SeekOrigin.SEEK_END);
        let nextSize = MpegParser.readAtomSize(br);
        const nextName = MpegParser.readAtomName(br);

        // If we do not have a parser for the next atom, skip the content and
        // read only the header (the one after the next).
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'Object'.
        if (!rootParser[nextName]) {
          filePos += nextSize - MpegParser.HEADER_SIZE;
          nextSize = MpegParser.HEADER_SIZE;
        }

        this.requestRead(
            rootParser, file, filePos, nextSize, nextName, onError, onSuccess);
      } else {
        // The previous read did not return the next atom header, EOF reached.
        this.vlog('EOF @' + filePos);
        onSuccess();
      }
    } catch (e) {
      // @ts-ignore: error TS18046: 'e' is of type 'unknown'.
      onError(e.toString());
    }
  }
}

/**
 * Size of the atom header.
 */
MpegParser.HEADER_SIZE = 8;
