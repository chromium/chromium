#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import io
import unittest
from unittest import mock

import diff
import file_format
import file_format_v2
import models


def _MakeSymbol(
  container,
  section_name,
  address,
  size,
  full_name,
  object_path='obj/foo.o',
  source_path='src/foo.cc',
  component='Blink',
  flags=0,
  padding=0,
  disassembly='',
):
  sym = models.Symbol(
    section_name,
    size,
    address=address,
    full_name=full_name,
    object_path=object_path,
    source_path=source_path,
    flags=flags,
    disassembly=disassembly,
  )
  sym.container = container
  sym.component = component
  sym.padding = padding
  sym.size = size + padding
  return sym


class FileFormatTest(unittest.TestCase):
  def testRoundTripSizeAndSizeDiffV1AndV2(self):
    for use_v2 in (False, True):
      c1 = models.Container(
        name='Base.apk',
        metadata={'apk_file_name': 'Base.apk'},
        section_sizes={'.text': 1000, '.rodata': 200},
        metrics_by_file={'libfoo.so': {'size/.text': 1000}},
      )
      c2 = models.Container(
        name='Other.apk',
        metadata={},
        section_sizes={'.text': 500},
        metrics_by_file={},
      )
      models.BaseContainer.AssignShortNames([c1, c2])

      s1 = _MakeSymbol(
        c1,
        '.text',
        0x1000,
        0x40,
        'foo::Bar()',
        flags=models.FLAG_STARTUP,
        disassembly='push %rbp\nret\n',
      )
      s2 = _MakeSymbol(
        c1,
        '.text',
        0x1000,
        0x40,
        'foo::BarAlias()',
        object_path='obj/bar.o',
        source_path='src/bar.cc',
        flags=models.FLAG_STARTUP,
      )
      s1.aliases = s2.aliases = [s1, s2]
      s3 = _MakeSymbol(
        c1,
        '.rodata',
        0x2000,
        0x20,
        models.STRING_LITERAL_NAME,
        flags=models.FLAG_ANONYMOUS,
      )
      s4 = _MakeSymbol(
        c2, '.text', 0x3000, 0x50, 'chrome::Main()', component='UI'
      )

      raw_symbols = [s1, s2, s3, s4]
      file_format.SortSymbols(raw_symbols)
      file_format.CalculatePadding(raw_symbols)
      size_info = models.SizeInfo(
        {'git_revision': 'abc123'}, [c1, c2], raw_symbols
      )

      buf = io.BytesIO()
      file_format.SaveSizeInfo(size_info, 't.size', file_obj=buf, use_v2=use_v2)
      data = buf.getvalue()
      if use_v2:
        self.assertTrue(data.startswith(file_format_v2.V2_HEADER_PREFIX))
      else:
        self.assertTrue(data.startswith(b'\x1f\x8b'))
        with gzip.GzipFile(fileobj=io.BytesIO(data)) as gz:
          self.assertIn(b'Size File Format v1.1\n', gz.read(64))

      buf.seek(0)
      loaded = file_format.LoadSizeInfo('t.size', file_obj=buf)
      self.assertEqual(size_info.build_config, loaded.build_config)
      self.assertEqual(2, len(loaded.containers))
      ls1, ls2, ls3, ls4 = loaded.raw_symbols
      self.assertEqual('foo::Bar()', ls1.full_name)
      self.assertEqual('foo::BarAlias()', ls2.full_name)
      self.assertIs(ls1.aliases, ls2.aliases)
      self.assertEqual('push %rbp\nret\n', ls1.disassembly)
      self.assertEqual('', ls2.disassembly)
      self.assertIs(models.STRING_LITERAL_NAME, ls3.full_name)
      self.assertEqual('UI', ls4.component)

      c_before = models.Container(
        name='',
        metadata={},
        section_sizes={'.text': 64, '.rodata': 32},
        metrics_by_file={},
      )
      c_after = models.Container(
        name='',
        metadata={},
        section_sizes={'.text': 112, '.rodata': 48},
        metrics_by_file={},
      )
      delta = diff.Diff(
        models.SizeInfo(
          {},
          [c_before],
          [
            _MakeSymbol(c_before, '.text', 0x1000, 0x20, 'f()'),
            _MakeSymbol(c_before, '.rodata', 0x2000, 0x10, 'kData'),
          ],
        ),
        models.SizeInfo(
          {},
          [c_after],
          [
            _MakeSymbol(c_after, '.text', 0x1000, 0x30, 'f()'),
            _MakeSymbol(c_after, '.text', 0x1030, 0x10, 'f2()'),
            _MakeSymbol(c_after, '.rodata', 0x2000, 0x18, 'kData'),
          ],
        ),
      )
      delta.removed_sources = ['old.cc']
      delta.added_sources = ['new.cc']

      diff_buf = io.BytesIO()
      file_format.SaveDeltaSizeInfo(
        delta, 't.sizediff', file_obj=diff_buf, use_v2=use_v2
      )
      diff_buf.seek(0)
      lb, la, rem, add = file_format.LoadDeltaSizeInfo(
        't.sizediff', file_obj=diff_buf
      )
      self.assertEqual(['old.cc'], rem)
      self.assertEqual(['new.cc'], add)
      self.assertEqual(
        {('f()', '.text', 0x20), ('kData', '.rodata', 0x10)},
        {(s.full_name, s.section_name, s.size) for s in lb.raw_symbols},
      )
      self.assertEqual(
        {
          ('f()', '.text', 0x30),
          ('f2()', '.text', 0x10),
          ('kData', '.rodata', 0x18),
        },
        {(s.full_name, s.section_name, s.size) for s in la.raw_symbols},
      )

  def testV2EmptyAndSparsePadding(self):
    c1 = models.Container(
      name='', metadata={}, section_sizes={'.text': 100}, metrics_by_file={}
    )
    empty_buf = io.BytesIO()
    file_format.SaveSizeInfo(
      models.SizeInfo({'git_revision': '0'}, [c1], []),
      'empty.size',
      file_obj=empty_buf,
      use_v2=True,
    )
    empty_buf.seek(0)
    self.assertEqual(
      0,
      len(
        file_format.LoadSizeInfo('empty.size', file_obj=empty_buf).raw_symbols
      ),
    )

    s1 = _MakeSymbol(c1, '.text', 0x1000, 0x10, 'a()', padding=12)
    s2 = _MakeSymbol(c1, '.text', 0, 0, 'Overhead: gap', padding=20)
    sparse_buf = io.BytesIO()
    file_format.SaveSizeInfo(
      models.SizeInfo({}, [c1], [s1, s2], is_sparse=True),
      'sparse.size',
      file_obj=sparse_buf,
      use_v2=True,
    )
    sparse_buf.seek(0)
    loaded = file_format.LoadSizeInfo(
      'sparse.size', file_obj=sparse_buf, is_sparse=True
    )
    self.assertEqual(
      (12, 0x10 + 12),
      (loaded.raw_symbols[0].padding, loaded.raw_symbols[0].size),
    )
    self.assertEqual(
      (20, 20), (loaded.raw_symbols[1].padding, loaded.raw_symbols[1].size)
    )

  def testV2MultiShardAndLcpBoundaries(self):
    c1 = models.Container(
      name='Base.apk',
      metadata={},
      section_sizes={'.text': 1000},
      metrics_by_file={},
    )
    models.BaseContainer.AssignShortNames([c1])
    names = [
      '',
      'x',
      'x' * 7 + 'a',
      'x' * 8 + 'b',
      'x' * 9 + 'c',
      'x' * 254 + 'd',
      'x' * 255 + 'e',
      'x' * 256 + 'f',
      'prefix_\u00e9_beta',
      'prefix_\u00e9_alpha',
    ]
    raw_symbols = [
      _MakeSymbol(c1, '.text', 0x1000 + i * 0x10, 0x10, name)
      for i, name in enumerate(names)
    ]
    size_info = models.SizeInfo({}, [c1], raw_symbols)
    buf = io.BytesIO()
    with mock.patch.object(file_format_v2, '_NAME_SHARD_SIZE', 2):
      file_format.SaveSizeInfo(
        size_info, 'sharded.size', file_obj=buf, use_v2=True
      )
    self.assertIn(b'names_0\x00', buf.getvalue())
    self.assertIn(b'names_4\x00', buf.getvalue())

    buf.seek(0)
    loaded = file_format.LoadSizeInfo('sharded.size', file_obj=buf)
    self.assertEqual(names, [s.full_name for s in loaded.raw_symbols])


if __name__ == '__main__':
  unittest.main()
