// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileType} from './file_type.m.js';
import {MockFileSystem} from './mock_entry.m.js';
import * as wrappedVolumeManagerCommon from '../../../base/js/volume_manager_types.m.js';
const {VolumeManagerCommon} = wrappedVolumeManagerCommon;
import {assertEquals} from 'chrome://test/chai_assert.js';

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
    {name: '/dir/foo.wav', want: {type: 'audio', subtype: 'WAV'}},
  ];
  for (const item of testItems) {
    const got = FileType.getTypeForName(item.name);
    assertEquals(item.want.type, got.type);
    assertEquals(item.want.subtype, got.subtype);
  }
}
