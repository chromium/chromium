// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {getFileTypeForName} from './file_types_base.js';

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
    const got = getFileTypeForName(item.name);
    assertEquals(item.want.type, got.type);
    assertEquals(item.want.subtype, got.subtype);
  }
}
