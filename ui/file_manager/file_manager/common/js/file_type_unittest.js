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
  return /** @type {!Entry} */ ({
    isDirectory: false,
    rootType: VolumeManagerCommon.RootType.MY_FILES,
    name: name,
    toURL: () => `filesyste:chrome://file-manager/root/${name}`,
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
  assertEquals('folder', FileType.getIcon(folder, mimetype, downloadsRoot));
  assertEquals('text', FileType.getIcon(fileA, mimetype, downloadsRoot));
  assertEquals('text', FileType.getIcon(fileB, mimetype, downloadsRoot));

  assertEquals(
      'downloads', FileType.getIcon(downloads, mimetype, downloadsRoot));
  assertEquals('folder', FileType.getIcon(downloads, mimetype, driveRoot));
  assertEquals('folder', FileType.getIcon(downloads, mimetype, androidRoot));
}

export function testGetTypeForName() {
  const testItems = [
    // Simple cases: file name only.
    {name: '/foo.amr', want: {type: 'audio', subtype: 'AMR'}},
    {name: '/foo.flac', want: {type: 'audio', subtype: 'FLAC'}},
    {name: '/foo.mp3', want: {type: 'audio', subtype: 'MP3'}},
    {name: '/foo.m4a', want: {type: 'audio', subtype: 'MPEG'}},
    {name: '/foo.oga', want: {type: 'audio', subtype: 'OGG'}},
    {name: '/foo.ogg', want: {type: 'audio', subtype: 'OGG'}},
    {name: '/foo.opus', want: {type: 'audio', subtype: 'OGG'}},
    {name: '/foo.wav', want: {type: 'audio', subtype: 'WAV'}},
    // Complex cases, directory and file name.
    {name: '/dir.mp3/foo.amr', want: {type: 'audio', subtype: 'AMR'}},
    {name: '/dir.ogg/foo.flac', want: {type: 'audio', subtype: 'FLAC'}},
    {name: '/dir/flac/foo.mp3', want: {type: 'audio', subtype: 'MP3'}},
    {name: '/dir_amr/foo.m4a', want: {type: 'audio', subtype: 'MPEG'}},
    {name: '/amr/foo.oga', want: {type: 'audio', subtype: 'OGG'}},
    {name: '/wav/dir/foo.ogg', want: {type: 'audio', subtype: 'OGG'}},
    {name: '/mp3/amr/foo.opus', want: {type: 'audio', subtype: 'OGG'}},
    // Archive types.
    {name: '/dir/foo.7z', want: {type: 'archive', subtype: '7-Zip'}},
    {name: '/dir/foo.crx', want: {type: 'archive', subtype: 'CRX'}},
    {name: '/dir/foo.iso', want: {type: 'archive', subtype: 'ISO'}},
    {name: '/dir/foo.rar', want: {type: 'archive', subtype: 'RAR'}},
    {name: '/dir/foo.tar', want: {type: 'archive', subtype: 'TAR'}},
    {name: '/dir/foo.zip', want: {type: 'archive', subtype: 'ZIP'}},
    {name: '/dir/foo.gz', want: {type: 'archive', subtype: 'GZIP'}},
    {name: '/dir/foo.tar.gz', want: {type: 'archive', subtype: 'GZIP'}},
    {name: '/dir/foo.tgz', want: {type: 'archive', subtype: 'GZIP'}},
    {name: '/dir/foo.lz', want: {type: 'archive', subtype: 'LZIP'}},
    {name: '/dir/foo.tar.lz', want: {type: 'archive', subtype: 'LZIP'}},
    {name: '/dir/foo.lzo', want: {type: 'archive', subtype: 'LZOP'}},
    {name: '/dir/foo.tar.lzo', want: {type: 'archive', subtype: 'LZOP'}},
    {name: '/dir/foo.lzma', want: {type: 'archive', subtype: 'LZMA'}},
    {name: '/dir/foo.tar.lzma', want: {type: 'archive', subtype: 'LZMA'}},
    {name: '/dir/foo.tlzma', want: {type: 'archive', subtype: 'LZMA'}},
    {name: '/dir/foo.tlz', want: {type: 'archive', subtype: 'LZMA'}},
    {name: '/dir/foo.bz', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.bz2', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.tar.bz', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.tar.bz2', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.tbz2', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.tbz', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.tb2', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.tz2', want: {type: 'archive', subtype: 'BZIP2'}},
    {name: '/dir/foo.xz', want: {type: 'archive', subtype: 'XZ'}},
    {name: '/dir/foo.txz', want: {type: 'archive', subtype: 'XZ'}},
    {name: '/dir/foo.tar.xz', want: {type: 'archive', subtype: 'XZ'}},
    {name: '/dir/foo.Z', want: {type: 'archive', subtype: 'Z'}},
    {name: '/dir/foo.tar.Z', want: {type: 'archive', subtype: 'Z'}},
    {name: '/dir/foo.taZ', want: {type: 'archive', subtype: 'Z'}},
    {name: '/dir/foo.tZ', want: {type: 'archive', subtype: 'Z'}},
    {name: '/dir/foo.zst', want: {type: 'archive', subtype: 'Zstandard'}},
    {name: '/dir/foo.tar.zst', want: {type: 'archive', subtype: 'Zstandard'}},
    {name: '/dir/foo.tzst', want: {type: 'archive', subtype: 'Zstandard'}},
    // Support upper case file name.
    {name: '/dir/foo.JPG', want: {type: 'image', subtype: 'JPEG'}},
    // Unknown files.
    {name: '/dir/foo', want: {type: 'UNKNOWN', subtype: ''}},
    {name: '/dir/foo.abc', want: {type: 'UNKNOWN', subtype: 'ABC'}},
  ];
  for (const item of testItems) {
    const got = FileType.getTypeForName(item.name);
    assertEquals(item.want.type, got.type);
    assertEquals(item.want.subtype, got.subtype);
  }
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
