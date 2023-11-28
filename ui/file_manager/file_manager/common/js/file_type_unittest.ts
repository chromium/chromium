// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {getIcon, getType, isDocument, isEncrypted} from './file_type.js';
import {MockFileSystem} from './mock_entry.js';
import {RootType} from './volume_manager_types.js';

function makeFakeEntry(name: string): Entry {
  return {
    isDirectory: false,
    rootType: RootType.MY_FILES,
    name: name,
    toURL: () => `filesystem:chrome://file-manager/root/${name}`,
  } as any as Entry;
}

function makeFakeDriveEntry(name: string): Entry {
  return {
    isDirectory: false,
    rootType: RootType.DRIVE,
    name: name,
    toURL: () =>
        `filesystem:chrome://file-manager/external/drivefs-aaaaa/root/${name}`,
  } as any as Entry;
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

  const folder = fileSystem.entries['/folder']!;
  const fileA = fileSystem.entries['/folder/file_a.txt']!;
  const downloads = fileSystem.entries['/Downloads']!;
  const fileB = fileSystem.entries['/Downloads/file_b.txt']!;

  const downloadsRoot = RootType.DOWNLOADS;
  const driveRoot = RootType.DRIVE;
  const androidRoot = RootType.ANDROID_FILES;

  const mimetype = undefined;
  assertEquals('folder', getIcon(folder, mimetype, downloadsRoot));
  assertEquals('text', getIcon(fileA, mimetype, downloadsRoot));
  assertEquals('text', getIcon(fileB, mimetype, downloadsRoot));

  assertEquals('downloads', getIcon(downloads, mimetype, downloadsRoot));
  assertEquals('folder', getIcon(downloads, mimetype, driveRoot));
  assertEquals('folder', getIcon(downloads, mimetype, androidRoot));
}

export function testIsDocument() {
  assertTrue(isDocument(makeFakeEntry('foo.txt')), '.txt');
  assertTrue(isDocument(makeFakeEntry('foo.csv')), '.csv');
  assertTrue(isDocument(makeFakeEntry('foo.doc')), '.doc');
  assertTrue(isDocument(makeFakeEntry('foo.docx')), '.docx');
  assertTrue(isDocument(makeFakeEntry('foo.gdoc')), '.gdoc');
  assertTrue(isDocument(makeFakeEntry('foo.gsheet')), '.gsheet');
  assertTrue(isDocument(makeFakeEntry('foo.gslides')), '.gslides');
  assertTrue(isDocument(makeFakeEntry('foo.gdraw')), '.gdraw');
  assertTrue(isDocument(makeFakeEntry('foo.pdf')), '.pdf');
  assertFalse(isDocument(makeFakeEntry('foo.png')), '.png');
  assertFalse(isDocument(makeFakeEntry('foo.ogg')), '.ogg');
  assertFalse(isDocument(makeFakeEntry('foo.zip')), '.zip');
  assertFalse(isDocument(makeFakeEntry('foo.qt')), '.qt');
}

export function testIsEncrypted() {
  assertTrue(isEncrypted(
      makeFakeDriveEntry('foo.gdoc'),
      'application/vnd.google-gsuite/encrypted; ' +
          'content="application/vnd.google-apps.document"'));
  assertTrue(isEncrypted(
      makeFakeDriveEntry('foo.txt'),
      'application/vnd.google-gsuite/encrypted; content="text/plain"'));
  assertFalse(isEncrypted(
      makeFakeDriveEntry('foo.gdoc'), 'application/vnd.google-apps.document'));
  assertFalse(isEncrypted(makeFakeDriveEntry('foo.txt'), 'text/plain'));
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
    const got = getType(entry, mimeType);
    assertEquals(item.want.type, got.type);
    assertEquals(item.want.subtype, got.subtype);
  }
}
