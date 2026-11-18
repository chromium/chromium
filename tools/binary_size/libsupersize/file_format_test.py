#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import gzip
import io
import json
import os
import pathlib
import tempfile
import unittest
from unittest import mock

import describe
import diff
import file_format
import file_format_v2
import models
import native_disassembly
import path_util
import readelf


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
  def testRoundTripSizeAndSizeDiff(self):
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
    file_format.SaveSizeInfo(size_info, 't.size', file_obj=buf)
    self.assertTrue(buf.getvalue().startswith(file_format_v2.V2_HEADER_PREFIX))

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
    file_format.SaveDeltaSizeInfo(delta, 't.sizediff', file_obj=diff_buf)
    self.assertEqual(
      2, diff_buf.getvalue().count(file_format_v2.V2_HEADER_PREFIX)
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

  def testLoadLegacyV1SizeAndSizeDiff(self):
    def frame(header_line, fields, body):
      fields_str = json.dumps(fields, sort_keys=True)
      return (
        f'# Created by //tools/binary_size\n{header_line}\n'
        f'{len(fields_str)}\n{fields_str}\n'.encode('ascii')
        + body
      )

    # Legacy v1 single-container .size (padding recomputed from addresses).
    v1_fields = {
      'has_components': True,
      'has_disassembly': True,
      'has_padding': False,
      'metadata': {
        'git_revision': 'deadbeef',
        'apk_file_name': 'Legacy.apk',
      },
      'section_sizes': {'.text': 256, '.rodata': 64},
      'metrics_by_file': {'lib.so': {'size/.text': 256}},
    }
    disasm_v1 = 'push %rbp\n// a · b\nret\n'
    disasm_v1_bytes = disasm_v1.encode('utf-8')
    v1_body = (
      '\n'.join(
        [
          '2',
          'obj/a.o\tsrc/a.cc',
          'obj/b.o\tsrc/b.cc',
          '2',
          'Blink',
          'UI',
          '.text\t.rodata',
          '3\t1',
          '4096 0 80',
          '8192',
          '64 64 32',
          '16',
          '0 1 -1',
          '1',
          '0 0 1',
          '0',
          f'foo::Bar()\t02\t{models.FLAG_STARTUP:x}',
          f'foo::BarAlias()\t{models.FLAG_STARTUP:x}',
          'foo::Padded()',
          f'{models.STRING_LITERAL_NAME}\t{models.FLAG_ANONYMOUS:x}',
          '0',
          f'{len(disasm_v1_bytes)}',
          '',
        ]
      ).encode('ascii')
      + disasm_v1_bytes
    )

    v1_gz = gzip.compress(frame('Size File Format v1', v1_fields, v1_body))
    v1_loaded = file_format.LoadSizeInfo(
      'legacy_v1.size', file_obj=io.BytesIO(v1_gz)
    )
    self.assertEqual({'git_revision': 'deadbeef'}, v1_loaded.build_config)
    self.assertEqual(1, len(v1_loaded.containers))
    self.assertEqual(
      {'apk_file_name': 'Legacy.apk'}, v1_loaded.containers[0].metadata
    )
    self.assertEqual(
      {'.text': 256, '.rodata': 64}, v1_loaded.containers[0].section_sizes
    )
    self.assertEqual(
      {'lib.so': {'size/.text': 256}}, v1_loaded.containers[0].metrics_by_file
    )
    s0, s1, s2, s3 = v1_loaded.raw_symbols
    self.assertEqual(
      ('foo::Bar()', 'obj/a.o', 'src/a.cc', 'Blink'),
      (s0.full_name, s0.object_path, s0.source_path, s0.component),
    )
    self.assertIs(s0.aliases, s1.aliases)
    self.assertEqual(2, len(s0.aliases))
    self.assertEqual(models.FLAG_STARTUP, s0.flags)
    self.assertEqual(disasm_v1, s0.disassembly)
    self.assertEqual('', s1.disassembly)
    self.assertEqual(
      ('foo::Padded()', 16, 48, 'UI'),
      (s2.full_name, s2.padding, s2.size, s2.component),
    )
    self.assertIs(models.STRING_LITERAL_NAME, s3.full_name)
    self.assertEqual(models.FLAG_ANONYMOUS, s3.flags)

    # Legacy v1.1 multi-container sparse .size files wrapped in a v1 .sizediff.
    v11_fields = {
      'build_config': {'git_revision': 'abc123'},
      'containers': [
        {
          'name': 'Base.apk',
          'metadata': {'apk_file_name': 'Base.apk'},
          'section_sizes': {'.text': 1000},
          'metrics_by_file': {},
        },
        {
          'name': 'Other.apk',
          'metadata': {},
          'section_sizes': {'.text': 500},
          'metrics_by_file': {},
        },
      ],
      'has_components': True,
      'has_disassembly': True,
      'has_padding': True,
    }

    def make_v11_sparse(text_size, disasm=''):
      disasm_bytes = disasm.encode('utf-8')
      tail = (
        f'0\n{len(disasm_bytes)}\n'.encode('ascii') + disasm_bytes
        if disasm_bytes
        else b'\n'
      )
      body = (
        '\n'.join(
          [
            '1',
            'obj/c.o\tsrc/c.cc',
            '1',
            'Core',
            '<0>.text\t<1>.text',
            '2\t1',
            '4096 0',
            '0',
            f'{text_size} {text_size}',
            '20',
            '12 12',
            '20',
            '0 0',
            '0',
            '0 0',
            '0',
            'foo::AliasA()\t02',
            'foo::AliasB()',
            'Overhead: gap',
          ]
        )
        + '\n'
      ).encode('ascii') + tail
      return gzip.compress(frame('Size File Format v1.1', v11_fields, body))

    before_gz = make_v11_sparse(0x20)
    after_gz = make_v11_sparse(0x30, disasm='nop\nret\n')
    removed_gz = gzip.compress(b'1\nold.cc\n')
    added_gz = gzip.compress(b'2\nnew1.cc\nnew2.cc\n')
    diff_fields = {
      'version': 1,
      'before_length': len(before_gz),
      'removed_sources_length': len(removed_gz),
      'added_sources_length': len(added_gz),
    }
    sizediff_bytes = frame(
      'DIFF', diff_fields, removed_gz + added_gz + before_gz + after_gz
    )
    lb, la, rem, add = file_format.LoadDeltaSizeInfo(
      'legacy.sizediff', file_obj=io.BytesIO(sizediff_bytes)
    )
    self.assertEqual(['old.cc'], rem)
    self.assertEqual(['new1.cc', 'new2.cc'], add)
    self.assertEqual(0x20 + 12, lb.raw_symbols[0].size)
    self.assertEqual(['Base.apk', 'Other.apk'], [c.name for c in la.containers])
    a0, a1, a2 = la.raw_symbols
    self.assertIs(a0.aliases, a1.aliases)
    self.assertEqual(
      (12, 0x30 + 12, 'nop\nret\n'), (a0.padding, a0.size, a0.disassembly)
    )
    self.assertEqual(
      ('Overhead: gap', 20, 20, 'Other.apk'),
      (a2.full_name, a2.padding, a2.size, a2.container.name),
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
      file_format.SaveSizeInfo(size_info, 'sharded.size', file_obj=buf)
    self.assertIn(b'names_0\x00', buf.getvalue())
    self.assertIn(b'names_4\x00', buf.getvalue())

    buf.seek(0)
    loaded = file_format.LoadSizeInfo('sharded.size', file_obj=buf)
    self.assertEqual(names, [s.full_name for s in loaded.raw_symbols])

  def testCollectRelocationsFromElf(self):
    mock_stdout = (
      "\nRelocation section '.rel.dyn' at offset 0x100 contains 2 entries:\n"
      " Offset     Info    Type                Sym. Value  Symbol's Name\n"
      '00001000  00000017 R_ARM_RELATIVE\n'
      '00001004  00000102 R_ARM_ABS32'
      '            00000000   sym_R_ARM_RELATIVE\n'
      '\n'
      "Relocation section '.rela.dyn' at offset 0x200 contains 3 entries:\n"
      '    Offset             Info             Type'
      "               Symbol's Value  Symbol's Name + Addend\n"
      '0000000000003000  0000000000000403 R_AARCH64_RELATIVE'
      '                10\n'
      '0000000000003008  0000000100000101 R_AARCH64_ABS64'
      '        0000000000000000 foo_RELATIVE + 0\n'
      '0000000000003010  0000000000000408 R_AARCH64_IRELATIVE'
      '               20\n'
      '\n'
      "Relocation section '.relr.dyn' at offset 0x300 contains 4 entries:\n"
      'Index: Entry            Address           Symbolic Address\n'
      '0000:  0000000000002000 0000000000002000  my_vtable\n'
      '0001:  000000000000000d 0000000000002010  my_vtable + 0x10\n'
      '                        0000000000002018  my_vtable + 0x18\n'
      '0002:  0000000000000001 0003:  0000000000002400 0000000000002400\n'
      '\n'
      "Relocation section '.android.relr' at offset 0x400 contains 3 entries:\n"
      'Index: Entry    Address  Symbolic Address\n'
      '0000:  00004000 00004000\n'
      '0001:  0000000b 00004004 kTable + 0x4\n'
      '                0000400c kTable + 0xc\n'
      '0002:  00000001 \n'
    )
    with mock.patch('subprocess.check_output', return_value=mock_stdout):
      addrs, total = readelf.CollectRelocationsFromElf('libtest.so')
    self.assertEqual(12, total)
    self.assertEqual(
      [
        0x1000,
        0x2000,
        0x2010,
        0x2018,
        0x2400,
        0x3000,
        0x4000,
        0x4004,
        0x400C,
      ],
      addrs,
    )

  def testRelocationsEncodeDecodeRoundTrip(self):
    # Empty list encodes to zero words and decodes to empty list.
    empty_words = file_format_v2._EncodeRelocations([])
    self.assertEqual([], list(empty_words))
    self.assertEqual([], file_format_v2._DecodeRelocations(empty_words))

    # addr == 0 requires offset == 0 right after marker 0.
    zero_words = file_format_v2._EncodeRelocations([0])
    self.assertEqual([0, 0], list(zero_words))
    self.assertEqual([0], file_format_v2._DecodeRelocations(zero_words))

    # Dense 4-byte run (32 entries fit in one 0xFFFFFFFF bitmap after base).
    dense_4 = [0x1000 + k * 4 for k in range(70)]
    dense_4_words = file_format_v2._EncodeRelocations(dense_4)
    self.assertEqual(
      [0, 0x1000, 0xFFFFFFFF, 0xFFFFFFFF], list(dense_4_words[:4])
    )
    self.assertEqual(5, len(dense_4_words))
    self.assertEqual(dense_4, file_format_v2._DecodeRelocations(dense_4_words))

    # Dense 8-byte run (16 entries per bitmap word with bit pattern 0xAAAAAAAA).
    dense_8 = [0x2000 + k * 8 for k in range(50)]
    dense_8_words = file_format_v2._EncodeRelocations(dense_8)
    self.assertEqual([0, 0x2000, 0xAAAAAAAA], list(dense_8_words[:3]))
    self.assertEqual(dense_8, file_format_v2._DecodeRelocations(dense_8_words))

    # Gaps within a window, across adjacent windows, and larger than 128 bytes.
    gaps = [
      0x3000,
      0x3000 + 3 * 4,
      0x3000 + 32 * 4,
      0x3000 + 50 * 4,
      0x3000 + 200 * 4,
    ]
    self.assertEqual(
      gaps,
      file_format_v2._DecodeRelocations(
        file_format_v2._EncodeRelocations(gaps)
      ),
    )

    # Odd/unaligned addresses mid-bitmap (causing negative wraparound offset
    # after bitmap window advance) followed by resumed bitmaps.
    odd_and_resumed = [
      0x5000,
      0x5004,
      0x5005,
      0x5009,
      0x500D,
      0x5011,
      0x5015,
      0x5018,
    ]
    odd_words = file_format_v2._EncodeRelocations(odd_and_resumed)
    self.assertEqual(
      odd_and_resumed, file_format_v2._DecodeRelocations(odd_words)
    )

    # High uint32 boundary addresses and duplicate deduplication.
    boundary = [
      0,
      0,
      0x1000,
      0x1000,
      0x80003000,
      0x80003004,
      0x80003011,
      0xFFFFFFA0,
      0xFFFFFFFC,
      0xFFFFFFFF,
    ]
    self.assertEqual(
      sorted(set(boundary)),
      file_format_v2._DecodeRelocations(
        file_format_v2._EncodeRelocations(boundary)
      ),
    )

    with self.assertRaises(AssertionError):
      file_format_v2._EncodeRelocations([0x1004, 0x1000])
    with self.assertRaises(ValueError):
      file_format_v2._DecodeRelocations([0])
    with self.assertRaises(ValueError):
      file_format_v2._DecodeRelocations([0, 0x1000, 0])
    # offset 0xFFFFFFFF after 0x1000 (cur=0x1004) validly yields 0x1003;
    # offset 0xFFFFFFFC repeats 0x1000 (not strictly increasing) and raises.
    self.assertEqual(
      [0x1000, 0x1003],
      file_format_v2._DecodeRelocations([0, 0x1000, 0, 0xFFFFFFFF]),
    )
    with self.assertRaises(ValueError):
      file_format_v2._DecodeRelocations([0, 0x1000, 0, 0xFFFFFFFC])
    with self.assertRaises(ValueError):
      file_format_v2._DecodeRelocations([0, 0x1000, 0, 0xFFFFFFFB])

  def testV2RelocationsRoundTripAndBssExclusion(self):
    container1 = models.Container(
      name='Base.apk',
      metadata={'apk_file_name': 'Base.apk', 'elf_arch': 'arm'},
      section_sizes={'.text': 1000, '.rodata': 200, '.tbss': 0x40},
      metrics_by_file={},
      relocation_addresses=[0x0804, 0x1004, 0x100C, 0x1011, 0x2000],
    )
    container2 = models.Container(
      name='Chrome.apk',
      metadata={'elf_arch': 'arm64'},
      section_sizes={'.data.rel.ro': 0x400},
      metrics_by_file={},
      relocation_addresses=[0x3000 + k * 8 for k in range(35)] + [0x3121],
    )
    models.BaseContainer.AssignShortNames([container1, container2])

    sym0 = _MakeSymbol(container1, '.text', 0x0800, 0x20, 'foo::Early()')
    sym1 = _MakeSymbol(container1, '.text', 0x1000, 0x40, 'foo::Bar()')
    sym2 = _MakeSymbol(container1, '.text', 0x1000, 0x40, 'foo::BarAlias()')
    sym1.aliases = sym2.aliases = [sym1, sym2]
    sym3 = _MakeSymbol(
      container1, '.rodata', 0x2000, 0x20, models.STRING_LITERAL_NAME
    )
    tbss_sym = _MakeSymbol(container1, '.tbss', 0x2000, 0x20, 'tls_var')
    sym4 = _MakeSymbol(container2, '.data.rel.ro', 0x3000, 0x200, 'kVtable')

    raw_symbols = [sym0, sym1, sym2, sym3, tbss_sym, sym4]
    file_format.SortSymbols(raw_symbols)
    file_format.CalculatePadding(raw_symbols)
    models.AttributeRelocations([container1, container2], raw_symbols)

    self.assertEqual(1, sym0.relocation_count)
    self.assertEqual(3, sym1.relocation_count)
    self.assertEqual(3, sym2.relocation_count)
    self.assertAlmostEqual(1.5, sym1.relocation_pss)
    self.assertEqual(1, sym3.relocation_count)
    self.assertEqual(0, tbss_sym.relocation_count)
    self.assertEqual(36, sym4.relocation_count)

    size_info = models.SizeInfo({}, [container1, container2], raw_symbols)
    buf = io.BytesIO()
    file_format.SaveSizeInfo(size_info, 'relocs.size', file_obj=buf)
    buf.seek(0)
    loaded = file_format.LoadSizeInfo('relocs.size', file_obj=buf)
    self.assertEqual(
      container1.relocation_addresses,
      loaded.containers[0].relocation_addresses,
    )
    self.assertEqual(
      container2.relocation_addresses,
      loaded.containers[1].relocation_addresses,
    )
    by_name = {sym.full_name: sym for sym in loaded.raw_symbols}
    self.assertEqual(1, by_name['foo::Early()'].relocation_count)
    self.assertEqual(3, by_name['foo::Bar()'].relocation_count)
    self.assertEqual(3, by_name['foo::BarAlias()'].relocation_count)
    self.assertEqual(1, by_name[models.STRING_LITERAL_NAME].relocation_count)
    self.assertEqual(0, by_name['tls_var'].relocation_count)
    self.assertEqual(36, by_name['kVtable'].relocation_count)

    # Verify MakeSparse filters relocation_addresses to kept symbols (even when
    # supplied out of address order) and leaves relocation_count intact on
    # containers with empty relocation_addresses.
    loaded.containers[1].relocation_addresses = []
    loaded.MakeSparse(
      [by_name['foo::Bar()'], by_name['foo::Early()'], by_name['kVtable']]
    )
    self.assertEqual(
      [0x0804, 0x1004, 0x100C, 0x1011],
      loaded.containers[0].relocation_addresses,
    )
    self.assertEqual([], loaded.containers[1].relocation_addresses)

    sparse_buf = io.BytesIO()
    file_format.SaveSizeInfo(loaded, 'sparse.size', file_obj=sparse_buf)
    sparse_buf.seek(0)
    reloaded_sparse = file_format.LoadSizeInfo(
      'sparse.size', file_obj=sparse_buf
    )
    self.assertTrue(reloaded_sparse.is_sparse)
    reloaded_by_name = {
      sym.full_name: sym for sym in reloaded_sparse.raw_symbols
    }
    self.assertEqual(3, reloaded_by_name['foo::Bar()'].relocation_count)
    self.assertEqual(1, reloaded_by_name['foo::Early()'].relocation_count)
    self.assertEqual(36, by_name['kVtable'].relocation_count)

    # Verify relocation-only diffs are tracked in DeltaSymbol but hidden in
    # text describe output when byte size is unchanged.
    cont_before = models.Container(
      name='',
      metadata={},
      section_sizes={'.data.rel.ro': 0x20},
      metrics_by_file={},
      relocation_addresses=[0x2000],
    )
    cont_after = models.Container(
      name='',
      metadata={},
      section_sizes={'.data.rel.ro': 0x20},
      metrics_by_file={},
      relocation_addresses=[0x2000, 0x2008],
    )
    sym_before = _MakeSymbol(cont_before, '.data.rel.ro', 0x2000, 0x20, 'kTbl')
    sym_after = _MakeSymbol(cont_after, '.data.rel.ro', 0x2000, 0x20, 'kTbl')
    models.AttributeRelocations([cont_before], [sym_before])
    models.AttributeRelocations([cont_after], [sym_after])
    reloc_delta = diff.Diff(
      models.SizeInfo({}, [cont_before], [sym_before]),
      models.SizeInfo({}, [cont_after], [sym_after]),
    )
    self.assertEqual(1, len(reloc_delta.raw_symbols))
    self.assertEqual(
      models.DIFF_STATUS_CHANGED, reloc_delta.raw_symbols[0].diff_status
    )
    self.assertEqual(1, reloc_delta.raw_symbols[0].relocation_count)
    self.assertIn(
      '0 changed (~)', '\n'.join(describe.GenerateLines(reloc_delta))
    )

  def testPathResolverAndAddSizeInfoDisassembly(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      sub_dir = pathlib.Path(tmp_dir) / 'sub'
      sub_dir.mkdir()
      elf_file = sub_dir / 'libtest.so'
      elf_file.write_bytes(b'\x7fELF')

      resolver = path_util.PathResolver(tmp_dir)
      self.assertEqual(elf_file, resolver('sub/libtest.so'))

      root_elf = pathlib.Path(tmp_dir) / 'libroot.so'
      root_elf.write_bytes(b'\x7fELF')
      self.assertEqual(root_elf, resolver('nested/dir/libroot.so'))

      with self.assertRaises(FileNotFoundError):
        resolver('missing.so')

      c_no_elf = models.Container(
        name='NoElf.apk',
        metadata={},
        section_sizes={'.text': 100},
        metrics_by_file={},
      )
      c_with_elf = models.Container(
        name='WithElf.apk',
        metadata={'elf_file_name': 'sub/libtest.so'},
        section_sizes={'.text': 100},
        metrics_by_file={},
      )
      models.BaseContainer.AssignShortNames([c_no_elf, c_with_elf])
      s_skip = _MakeSymbol(c_no_elf, '.text', 0x1000, 500, 'skip::Func()')
      s_keep = _MakeSymbol(c_with_elf, '.text', 0x2000, 100, 'keep::Func()')
      size_info = models.SizeInfo({}, [c_no_elf, c_with_elf], [s_skip, s_keep])

      seen_out_dirs = []

      @contextlib.contextmanager
      def fake_disassemble(sym, out_dir, elf_path):
        seen_out_dirs.append(out_dir)
        yield [f'disasm {sym.full_name} from {os.path.basename(elf_path)}\n']

      with mock.patch.object(
        native_disassembly, 'Disassemble', side_effect=fake_disassemble
      ):
        native_disassembly.AddSizeInfoDisassembly(
          size_info, resolver, output_directory=str(sub_dir)
        )
      self.assertEqual([str(sub_dir)], seen_out_dirs)
      self.assertEqual('', s_skip.disassembly)
      self.assertEqual(
        'disasm keep::Func() from libtest.so\n', s_keep.disassembly
      )

      # Verify build_config['out_directory'] resolves relative to tools src root
      # rather than CWD, and --no-output-directory (None) stays None.
      fake_src = pathlib.Path(tmp_dir) / 'fake_src'
      real_out = fake_src / 'out' / 'Real'
      real_out.mkdir(parents=True)
      decoy_cwd = pathlib.Path(tmp_dir) / 'decoy_cwd'
      (decoy_cwd / 'out' / 'Decoy').mkdir(parents=True)
      with (
        contextlib.chdir(decoy_cwd),
        mock.patch.object(path_util, '_TOOLS_SRC_ROOT', str(fake_src)),
      ):
        size_info_decoy = models.SizeInfo(
          {models.BUILD_CONFIG_OUT_DIRECTORY: 'out/Decoy'},
          [c_with_elf],
          [s_keep],
        )
        self.assertIsNone(
          native_disassembly._ResolveOutputDirectory(size_info_decoy, None)
        )
        size_info_real = models.SizeInfo(
          {models.BUILD_CONFIG_OUT_DIRECTORY: 'out/Real'},
          [c_with_elf],
          [s_keep],
        )
        self.assertEqual(
          os.path.relpath(real_out, decoy_cwd),
          native_disassembly._ResolveOutputDirectory(size_info_real, None),
        )


if __name__ == '__main__':
  unittest.main()
