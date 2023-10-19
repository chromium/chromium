// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {FileType} from './file_type.js';
import {MockFileSystem} from './mock_entry.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * @param {string} name
 * @return {!Entry}
 */
function makeFakeEntry(name) {
  // @ts-ignore: error TS2352: Conversion of type '{ isDirectory: false;
  // rootType: string; name: string; toURL: () => string; }' to type
  // 'FileSystemEntry' may be a mistake because neither type sufficiently
  // overlaps with the other. If this was intentional, convert the expression to
  // 'unknown' first.
  return /** @type {!Entry} */ ({
    isDirectory: false,
    rootType: VolumeManagerCommon.RootType.MY_FILES,
    name: name,
    toURL: () => `filesystem:chrome://file-manager/root/${name}`,
  });
}

/**
 * @param {string} name
 * @return {!Entry}
 */
function makeFakeDriveEntry(name) {
  // @ts-ignore: error TS2352: Conversion of type '{ isDirectory: false;
  // rootType: string; name: string; toURL: () => string; }' to type
  // 'FileSystemEntry' may be a mistake because neither type sufficiently
  // overlaps with the other. If this was intentional, convert the expression to
  // 'unknown' first.
  return /** @type {!Entry} */ ({
    isDirectory: false,
    rootType: VolumeManagerCommon.RootType.DRIVE,
    name: name,
    toURL: () =>
        `filesystem:chrome://file-manager/external/drivefs-aaaaa/root/${name}`,
  });
}

/*
 * Tests that Downloads icon is customized within Downloads root, but not in
 * others.
 */
export function testDownloadsIcon() {
  const fileSystem = new MockFileSystem('fake-fs');
  const filenames = [
    '/folder/',
    '/folder/file_a.txt',
    '/Downloads/',
    '/Downloads/file_b.txt',
  ];
  fileSystem.populate(filenames);

  const folder = fileSystem.entries['/folder'];
  const fileA = fileSystem.entries['/folder/file_a.txt'];
  const downloads = fileSystem.entries['/Downloads'];
  const fileB = fileSystem.entries['/Downloads/file_b.txt'];

  const downloadsRoot = VolumeManagerCommon.RootType.DOWNLOADS;
  const driveRoot = VolumeManagerCommon.RootType.DRIVE;
  const androidRoot = VolumeManagerCommon.RootType.ANDROID_FILES;

  const mimetype = undefined;
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined' is
  // not assignable to parameter of type 'FileSystemEntry | VolumeEntry |
  // FileData'.
  assertEquals('folder', FileType.getIcon(folder, mimetype, downloadsRoot));
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined' is
  // not assignable to parameter of type 'FileSystemEntry | VolumeEntry |
  // FileData'.
  assertEquals('text', FileType.getIcon(fileA, mimetype, downloadsRoot));
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined' is
  // not assignable to parameter of type 'FileSystemEntry | VolumeEntry |
  // FileData'.
  assertEquals('text', FileType.getIcon(fileB, mimetype, downloadsRoot));

  assertEquals(
      // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
      // undefined' is not assignable to parameter of type 'FileSystemEntry |
      // VolumeEntry | FileData'.
      'downloads', FileType.getIcon(downloads, mimetype, downloadsRoot));
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined' is
  // not assignable to parameter of type 'FileSystemEntry | VolumeEntry |
  // FileData'.
  assertEquals('folder', FileType.getIcon(downloads, mimetype, driveRoot));
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined' is
  // not assignable to parameter of type 'FileSystemEntry | VolumeEntry |
  // FileData'.
  assertEquals('folder', FileType.getIcon(downloads, mimetype, androidRoot));
}

export function testIsDocument() {
  assertTrue(FileType.isDocument(makeFakeEntry('foo.txt')), '.txt');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.csv')), '.csv');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.doc')), '.doc');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.docx')), '.docx');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.gdoc')), '.gdoc');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.gsheet')), '.gsheet');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.gslides')), '.gslides');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.gdraw')), '.gdraw');
  assertTrue(FileType.isDocument(makeFakeEntry('foo.pdf')), '.pdf');
  assertFalse(FileType.isDocument(makeFakeEntry('foo.png')), '.png');
  assertFalse(FileType.isDocument(makeFakeEntry('foo.ogg')), '.ogg');
  assertFalse(FileType.isDocument(makeFakeEntry('foo.zip')), '.zip');
  assertFalse(FileType.isDocument(makeFakeEntry('foo.qt')), '.qt');
}

export function testIsEncrypted() {
  assertTrue(FileType.isEncrypted(
      makeFakeDriveEntry('foo.gdoc'),
      'application/vnd.google-gsuite/encrypted; ' +
          'content="application/vnd.google-apps.document"'));
  assertTrue(FileType.isEncrypted(
      makeFakeDriveEntry('foo.txt'),
      'application/vnd.google-gsuite/encrypted; content="text/plain"'));
  assertFalse(FileType.isEncrypted(
      makeFakeDriveEntry('foo.gdoc'), 'application/vnd.google-apps.document'));
  assertFalse(
      FileType.isEncrypted(makeFakeDriveEntry('foo.txt'), 'text/plain'));
}

export function testEncryptedTypeDetection() {
  const testItems = [
    // Guess by mime type only, name won't give a hint.
    {
      name: 'foo',
      mime: 'application/pdf',
      want: {type: 'document', subtype: 'PDF'},
    },
    {name: 'foo', mime: 'audio/flac', want: {type: 'audio', subtype: 'FLAC'}},
    {name: 'foo', mime: 'image/png', want: {type: 'image', subtype: 'PNG'}},
    {name: 'foo', mime: 'text/plain', want: {type: 'text', subtype: 'TXT'}},
    // Guess by name only.
    {
      name: 'foo.pdf',
      mime: 'unknown',
      want: {type: 'document', subtype: 'PDF'},
    },
    {name: 'foo.flac', mime: 'unknown', want: {type: 'audio', subtype: 'FLAC'}},
    {name: 'foo.png', mime: 'unknown', want: {type: 'image', subtype: 'PNG'}},
    {name: 'foo.txt', mime: 'unknown', want: {type: 'text', subtype: 'TXT'}},
    // Guess by both factors.
    {
      name: 'foo.pdf',
      mime: 'application/pdf',
      want: {type: 'document', subtype: 'PDF'},
    },
    {
      name: 'foo.flac',
      mime: 'audio/flac',
      want: {type: 'audio', subtype: 'FLAC'},
    },
    {name: 'foo.png', mime: 'image/png', want: {type: 'image', subtype: 'PNG'}},
    {name: 'foo.txt', mime: 'text/plain', want: {type: 'text', subtype: 'TXT'}},
    // Guess by both factors possible, but names are misleading.
    {
      name: 'foo.ogg',
      mime: 'application/pdf',
      want: {type: 'document', subtype: 'PDF'},
    },
    {
      name: 'foo.docx',
      mime: 'audio/flac',
      want: {type: 'audio', subtype: 'FLAC'},
    },
    {name: 'foo.pdf', mime: 'image/png', want: {type: 'image', subtype: 'PNG'}},
    {
      name: 'foo.jpeg',
      mime: 'text/plain',
      want: {type: 'text', subtype: 'TXT'},
    },
  ];
  for (const item of testItems) {
    const entry = makeFakeDriveEntry(item.name);
    const mimeType =
        `application/vnd.google-gsuite/encrypted; content="${item.mime}"`;
    const got = FileType.getType(entry, mimeType);
    assertEquals(item.want.type, got.type);
    assertEquals(item.want.subtype, got.subtype);
  }
}
