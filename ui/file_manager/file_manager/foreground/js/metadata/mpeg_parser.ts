// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ByteReader, SeekOrigin} from './byte_reader.js';
import type {ParserMetadata} from './metadata_item.js';
import {MetadataParser, type MetadataParserLogger} from './metadata_parser.js';

interface Atom {
  name: string;
  parent?: Atom;

  start: number;
  end: number;

  trackType?: string;
}

type AtomParser = ((br: ByteReader, atom: Atom) => void);

interface ParseTree {
  [name: string]: AtomParser|ParseTree|boolean|undefined;
  versioned?: boolean;
}

export class MpegParser extends MetadataParser {
  constructor(parent: MetadataParserLogger) {
    super(parent, 'mpeg', /\.(mp4|m4v|m4a|mpe?g4?)$/i);
    this.mimeType = 'video/mpeg';
  }

  /**
   * @param br ByteReader instance.
   * @param end End of atom position.
   * @return Atom size.
   */
  static readAtomSize(br: ByteReader, end?: number): number {
    const pos = br.tell();

    if (end) {
      // Assert that opt_end <= buffer end.
      // When supplied, opt_end is the end of the enclosing atom and is used to
      // check the correct nesting.
      br.validateRead(end - pos);
    }

    const size = br.readScalar(4, false, end);

    if (size < MpegParser.HEADER_SIZE) {
      throw new Error('atom too short (' + size + ') @' + pos);
    }

    if (end && pos + size > end) {
      throw new Error(
          'atom too long (' + size + '>' + (end - pos) + ') @' + pos);
    }

    return size;
  }

  /**
   * @param br ByteReader instance.
   * @param end End of atom position.
   * @return Atom name.
   */
  static readAtomName(br: ByteReader, end?: number): string {
    return br.readString(4, end).toLowerCase();
  }

  /**
   * @param metadata Metadata object.
   * @return Root of the parser tree.
   */
  static createRootParser(metadata: ParserMetadata): ParseTree {
    function findParentAtom(atom: Atom, name: string) {
      for (;;) {
        if (!atom.parent) {
          return null;
        }
        atom = atom.parent;
        if (atom.name === name) {
          return atom;
        }
      }
    }

    function parseFtyp(br: ByteReader, atom: Atom) {
      metadata.mpegBrand = br.readString(4, atom.end);
    }

    function parseMvhd(br: ByteReader, atom: Atom) {
      const version = br.readScalar(4, false, atom.end);
      const offset = (version === 0) ? 8 : 16;
      br.seek(offset, SeekOrigin.SEEK_CUR);
      const timescale = br.readScalar(4, false, atom.end);
      const duration = br.readScalar(4, false, atom.end);
      metadata.duration = duration / timescale;
    }

    function parseHdlr(br: ByteReader, atom: Atom) {
      br.seek(8, SeekOrigin.SEEK_CUR);
      const type = br.readString(4, atom.end);
      const track = findParentAtom(atom, 'trak');
      if (track) {
        track.trackType = type;
      }
    }

    function parseStsd(br: ByteReader, atom: Atom) {
      const track = findParentAtom(atom, 'trak');
      if (track && track.trackType === 'vide') {
        br.seek(40, SeekOrigin.SEEK_CUR);
        metadata.width = br.readScalar(2, false, atom.end);
        metadata.height = br.readScalar(2, false, atom.end);
      }
    }

    function parseDataString(
        name: 'title'|'album'|'artist', br: ByteReader, atom: Atom) {
      br.seek(8, SeekOrigin.SEEK_CUR);
      metadata[name] = br.readString(atom.end - br.tell(), atom.end);
    }

    function parseCovr(br: ByteReader, atom: Atom) {
      br.seek(8, SeekOrigin.SEEK_CUR);
      metadata.thumbnailURL = br.readImage(atom.end - br.tell(), atom.end);
    }

    // 'meta' atom can occur at one of the several places in the file structure.
    const parseMeta: ParseTree = {
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
   * @param file File.
   * @param metadata Metadata.
   * @param callback Success callback.
   * @param onError Error callback.
   */
  parse(
      file: File, metadata: ParserMetadata,
      callback: (metadata: ParserMetadata) => void,
      onError: (error: (ProgressEvent|string)) => void) {
    const rootParser = MpegParser.createRootParser(metadata);

    // Kick off the processing by reading the first atom's header.
    this.requestRead(
        rootParser, file, 0, MpegParser.HEADER_SIZE, null, onError,
        callback.bind(null, metadata));
  }

  /**
   * @param parser Parser tree node.
   * @param br ByteReader instance.
   * @param atom Atom descriptor.
   * @param filePos File position of the atom start.
   */
  applyParser(
      parser: ParseTree[string], br: ByteReader, atom: Atom, filePos: number) {
    if (this.verbose) {
      let path = atom.name;
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

      const start = atom.start - MpegParser.HEADER_SIZE;
      this.vlog(
          path + ': ' +
              '@' + (filePos + start) + ':' + (atom.end - start),
          action);
    }

    if (parser instanceof Function) {
      br.pushSeek(atom.start);
      parser(br, atom);
      br.popSeek();
    } else if (parser instanceof Object) {
      if (parser.versioned) {
        atom.start += 4;
      }
      this.parseMpegAtomsInRange(parser, br, atom, filePos);
    }
  }

  /**
   * @param parser Parser tree node.
   * @param br ByteReader instance.
   * @param parentAtom Parent atom descriptor.
   * @param filePos File position of the atom start.
   */
  parseMpegAtomsInRange(
      parser: ParseTree, br: ByteReader, parentAtom: Atom, filePos: number) {
    let count = 0;
    for (let offset = parentAtom.start; offset !== parentAtom.end;) {
      if (count++ > 100) {
        // Most likely we are looping through a corrupt file.
        throw new Error(
            'too many child atoms in ' + parentAtom.name + ' @' + offset);
      }

      br.seek(offset);
      const size = MpegParser.readAtomSize(br, parentAtom.end);
      const name = MpegParser.readAtomName(br, parentAtom.end);

      this.applyParser(
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
   * @param rootParser Parser definition.
   * @param file File.
   * @param filePos Start position in the file.
   * @param size Atom size.
   * @param name Atom name.
   * @param onError Error callback.
   * @param onSuccess Success callback.
   */
  requestRead(
      rootParser: ParseTree, file: File, filePos: number, size: number,
      name: string|null, onError: (error: (ProgressEvent|string)) => void,
      onSuccess: () => void) {
    const self = this;
    const reader = new FileReader();
    reader.onerror = onError;
    reader.onload = _event => {
      self.processTopLevelAtom(
          reader.result as ArrayBuffer, rootParser, file, filePos, size, name,
          onError, onSuccess);
    };
    this.vlog('reading @' + filePos + ':' + size);
    reader.readAsArrayBuffer(file.slice(filePos, filePos + size));
  }

  /**
   * @param buf Data buffer.
   * @param rootParser Parser definition.
   * @param file File.
   * @param filePos Start position in the file.
   * @param size Atom size.
   * @param name Atom name.
   * @param onError Error callback.
   * @param onSuccess Success callback.
   */
  processTopLevelAtom(
      buf: ArrayBuffer, rootParser: ParseTree, file: File, filePos: number,
      size: number, name: string|null,
      onError: (error: (ProgressEvent|string)) => void, onSuccess: () => void) {
    try {
      const br = new ByteReader(buf);

      // the header has already been read.
      const atomEnd = size - MpegParser.HEADER_SIZE;

      const bufLength = buf.byteLength;

      // Check the available data size. It should be either exactly
      // what we requested or HEADER_SIZE bytes less (for the last atom).
      if (bufLength !== atomEnd && bufLength !== size) {
        throw new Error(
            'Read failure @' + filePos + ', ' +
            'requested ' + size + ', read ' + bufLength);
      }

      // Process the top level atom.
      if (name) {  // name is null only the first time.
        this.applyParser(
            rootParser[name], br, {start: 0, end: atomEnd, name: name},
            filePos);
      }

      filePos += bufLength;
      if (bufLength === size) {
        // The previous read returned everything we asked for, including
        // the next atom header at the end of the buffer.
        // Parse this header and schedule the next read.
        br.seek(-MpegParser.HEADER_SIZE, SeekOrigin.SEEK_END);
        let nextSize = MpegParser.readAtomSize(br);
        const nextName = MpegParser.readAtomName(br);

        // If we do not have a parser for the next atom, skip the content and
        // read only the header (the one after the next).
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
      onError(e!.toString());
    }
  }

  /**
   * Size of the atom header.
   */
  static readonly HEADER_SIZE = 8;
}
