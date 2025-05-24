// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {Id3Parser} from './id3_parser.js';
import {ParserMetadata} from './metadata_item.js';
import type {MetadataParserLogger} from './metadata_parser.js';

class ConsoleLogger implements MetadataParserLogger {
  verbose = true;

  error(...args: Array<Object|string>) {
    console.error(...args);
  }

  log(...args: Array<Object|string>) {
    console.info(...args);
  }

  vlog(...args: Array<Object|string>) {
    console.info(...args);
  }
}

export async function testEmptyFile() {
  const parent = new ConsoleLogger();
  const parser = new Id3Parser(parent);

  const file = new File([], 'test');
  const metadata = new ParserMetadata();

  try {
    await new Promise<ParserMetadata>((resolve, reject) => {
      parser.parse(file, metadata, resolve, reject);
    });
    assertNotReached();
  } catch (e: unknown) {
    assertTrue((e as string).includes('Invalid read position'));
  }
}

export async function testNonId3File() {
  const parent = new ConsoleLogger();
  const parser = new Id3Parser(parent);

  const file = new File(['TA_'], 'test');
  const metadata = new ParserMetadata();
  const originalMetadata = structuredClone(metadata);

  const result = await new Promise<ParserMetadata>((resolve, reject) => {
    parser.parse(file, metadata, resolve, reject);
  });
  assertDeepEquals(result, originalMetadata);
}

export async function testId3v1File() {
  const parent = new ConsoleLogger();
  const parser = new Id3Parser(parent);

  const fileContent =
      ('TAG' +
       'this is the title\0'.padEnd(30) + 'this is the artist\0'.padEnd(30) +
       'this is the album\0'.padEnd(30))
          .padEnd(128);

  const file = new File([fileContent], 'test');
  const metadata = new ParserMetadata();

  const result = await new Promise<ParserMetadata>((resolve, reject) => {
    parser.parse(file, metadata, resolve, reject);
  });
  assertDeepEquals(result, {
    title: 'this is the title',
    artist: 'this is the artist',
    album: 'this is the album',
  });
}

export async function testId3v2Major1WithNoFramesFile() {
  const parent = new ConsoleLogger();
  const parser = new Id3Parser(parent);

  const fileContent = 'ID3' +
      '\x01\x01\x00' +      // major, minor, flags
      '\x00\x00\x00\x04' +  // size
      '\0'.padEnd(4);       // empty frame name

  const file = new File([fileContent], 'test');
  const metadata = new ParserMetadata();

  const result = await new Promise<ParserMetadata>((resolve, reject) => {
    parser.parse(file, metadata, resolve, reject);
  });
  assertDeepEquals(result, {
    id3v2: {
      majorVersion: 1,
      minorVersion: 1,
      flags: 0,
      size: 4,
      frames: {},
    },
    description: [],
  });
}

export async function testId3v2Major4WithFramesFile() {
  const parent = new ConsoleLogger();
  const parser = new Id3Parser(parent);

  const fileContent = 'ID3' +
      '\x04\x00\x00' +      // major, minor, flags
      '\x00\x00\x00\x4E' +  // size

      // Frame header
      'TALB' +              // frame name
      '\x00\x00\x00\x0A' +  // frame size (excl. header of 10)
      '\x00\x00' +          // frame flags
      // Frame
      '\x00' +        // encoding
      'The Wall\0' +  // frame content

      // Frame header
      'TIT2' +              // frame name
      '\x00\x00\x00\x1B' +  // frame size (excl. header of 10)
      '\x00\x00' +          // frame flags
      // Frame
      '\x00' +                         // encoding
      'Another Brick in the Wall\0' +  // frame content

      // Frame header
      'TPE1' +              // frame name
      '\x00\x00\x00\x0B' +  // frame size (excl. header of 10)
      '\x00\x00' +          // frame flags
      // Frame
      '\x00' +         // encoding
      'Pink Floyd\0';  // frame content

  const file = new File([fileContent], 'test');
  const metadata = new ParserMetadata();

  const result = await new Promise<ParserMetadata>((resolve, reject) => {
    parser.parse(file, metadata, resolve, reject);
  });
  assertDeepEquals(result, {
    id3v2: {
      majorVersion: 4,
      minorVersion: 0,
      flags: 0,
      size: 78,
      frames: {
        'TALB': {
          name: 'TALB',
          size: 10,
          headerSize: 10,
          flags: 0,
          encoding: 0,
          value: 'The Wall',
        },
        'TIT2': {
          name: 'TIT2',
          size: 27,
          headerSize: 10,
          flags: 0,
          encoding: 0,
          value: 'Another Brick in the Wall',
        },
        'TPE1': {
          name: 'TPE1',
          size: 11,
          headerSize: 10,
          flags: 0,
          encoding: 0,
          value: 'Pink Floyd',
        },
      },
    },
    description: [
      {
        key: 'ID3_TITLE',
        value: 'Another Brick in the Wall',
      },
      {
        key: 'ID3_LEAD_PERFORMER',
        value: 'Pink Floyd',
      },
      {
        key: 'ID3_ALBUM',
        value: 'The Wall',
      },
    ],
    album: 'The Wall',
    title: 'Another Brick in the Wall',
    artist: 'Pink Floyd',
  });
}
