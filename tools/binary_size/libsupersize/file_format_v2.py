# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Deals with loading & saving .size v2 files.

See docs/file_format.md for a specification of the v2 binary format.
"""

import array
from compression import zstd
import concurrent.futures
import contextlib
import itertools
import json
import os
import pickle
import struct
import sys
import tempfile
import threading
import traceback

import file_format
import models

# Fixed-width array.array byte-plane splitting assumes native little-endian.
assert sys.byteorder == 'little'

_COMMON_HEADER = b'# Created by //tools/binary_size\n'
_SIZE_HEADER_V2 = b'Size File Format v2\n'
# 56-byte magic prefix: 33B common header + 20B v2 header + 3B reserved zeros.
# Followed by uint32 header_flags + uint32 num_blobs = 64-byte file header.
V2_HEADER_PREFIX = _COMMON_HEADER + _SIZE_HEADER_V2 + b'\x00\x00\x00'

# TOC entry: 16B NUL-padded ASCII name, u64 offset, u64 comp_size, u64 raw_size.
_TOC_ENTRY_STRUCT = struct.Struct('<16sQQQ')

_FLAG_HAS_PADDING = 1
_NAME_SHARD_SIZE = 262144


def _ByteStreamSplit(arr):
  """Transposes an array of 4-byte integers into 4 contiguous byte planes."""
  raw = arr.tobytes()
  return b''.join(raw[b::4] for b in range(4))


def _ByteStreamUnsplit(data, typecode, count):
  """Reconstructs an array of |count| 4-byte integers from 4 byte planes."""
  out = bytearray(count * 4)
  for b in range(4):
    out[b::4] = data[b * count : (b + 1) * count]
  arr = array.array(typecode)
  arr.frombytes(out)
  return arr


def _SplitZigZagDeltas(values):
  """Encodes 32-bit signed deltas of |values| with ZigZag + byte-splitting."""
  first = values[0]
  arr = array.array('I', [((first << 1) ^ (first >> 31)) & 0xFFFFFFFF])
  arr.extend(
    (((b - a) << 1) ^ ((b - a) >> 31)) & 0xFFFFFFFF
    for a, b in itertools.pairwise(values)
  )
  return _ByteStreamSplit(arr)


def _UnsplitZigZagDeltas(data, count):
  """Decodes a byte-split ZigZag delta stream into prefix-summed ints."""
  return itertools.accumulate(
    ((u >> 1) ^ -(u & 1)) for u in _ByteStreamUnsplit(data, 'I', count)
  )


def _ZstdCompress(data):
  """Compresses |data| using Zstandard (level 9, window_log 25)."""
  return zstd.compress(
    data,
    options={
      zstd.CompressionParameter.compression_level: 9,
      zstd.CompressionParameter.window_log: 25,
    },
  )


def _CompressNamedBlobs(named_blobs):
  """Compresses a list of (name, raw_bytes) pairs in parallel with Zstd."""
  with concurrent.futures.ThreadPoolExecutor(
    max_workers=len(named_blobs)
  ) as ex:
    return [
      (name, len(raw), comp)
      for (name, raw), comp in zip(
        named_blobs, ex.map(_ZstdCompress, (b[1] for b in named_blobs))
      )
    ]


def _EncodeNameShard(syms, shard_idx, start_idx, end_idx):
  """Encodes a symbol name shard via shard-local permutation + front-coding."""
  sym_count = end_idx - start_idx
  slice_names = [s.full_name for s in syms[start_idx:end_idx]]
  perm = sorted(range(sym_count), key=slice_names.__getitem__)

  lcp_arr = bytearray(sym_count)
  suffixes = []
  append_suf = suffixes.append
  prev = b''
  for idx, rank in enumerate(perm):
    cur = slice_names[rank].encode('utf-8', errors='surrogatepass')
    n = min(len(prev), len(cur), 255)
    if n:
      # Big-endian XOR places byte 0 in the MSB so bit_length() locates the
      # first differing byte directly in C.
      xor = int.from_bytes(prev[:n], 'big') ^ int.from_bytes(cur[:n], 'big')
      lcp = n if not xor else n - ((xor.bit_length() + 7) >> 3)
    else:
      lcp = 0
    lcp_arr[idx] = lcp
    append_suf(cur[lcp:])
    prev = cur
  raw = (
    struct.pack('<II', start_idx, sym_count)
    + _SplitZigZagDeltas(perm)
    + bytes(lcp_arr)
    + b'\x00'.join(suffixes)
    + b'\x00'
  )
  return (f'names_{shard_idx}', len(raw), _ZstdCompress(raw))


def _BuildMetaAndExtras(size_info, syms):
  """Builds 'meta', optional 'aliases', and optional 'disassembly' blobs."""
  num_containers = len(size_info.containers)
  total_symbols = len(syms)
  c_to_idx = {c.name: i for i, c in enumerate(size_info.containers)}
  segments = [
    [c_to_idx[c_name], sec_name, sum(1 for _ in grp)]
    for (c_name, sec_name), grp in itertools.groupby(
      syms, key=lambda s: (s.container.name, s.section_name)
    )
  ]

  meta_obj = {
    'build_config': size_info.build_config,
    'containers': [
      {
        'name': '' if num_containers == 1 else c.name,
        'metadata': c.metadata,
        'section_sizes': c.section_sizes,
        'metrics_by_file': c.metrics_by_file,
      }
      for c in size_info.containers
    ],
    'segments': segments,
    'total_symbols': total_symbols,
  }
  out = [('meta', json.dumps(meta_obj, sort_keys=True).encode('utf-8'))]

  if total_symbols > 0:
    alias_firsts = array.array('I')
    alias_counts = array.array('I')
    idx = prev_first = 0
    while idx < total_symbols:
      aliases = syms[idx].aliases
      if aliases:
        cnt = len(aliases)
        alias_firsts.append(idx - prev_first)
        alias_counts.append(cnt)
        prev_first = idx
        idx += cnt
      else:
        idx += 1
    if alias_firsts:
      out.append(
        (
          'aliases',
          struct.pack('<I', len(alias_firsts))
          + _ByteStreamSplit(alias_firsts)
          + _ByteStreamSplit(alias_counts),
        )
      )

    disasm = [
      struct.pack('<II', i, len(b)) + b
      for i, s in enumerate(syms)
      if s.disassembly
      for b in (s.disassembly.encode('utf-8', errors='surrogatepass'),)
    ]
    if disasm:
      out.append(
        ('disassembly', struct.pack('<I', len(disasm)) + b''.join(disasm))
      )
  return _CompressNamedBlobs(out)


def _BuildPathsBlobs(syms):
  """Builds deduplicated 'paths' (including components) and 'path_indices'."""
  triples = []
  triple_to_idx = {}
  comps = []
  comp_to_idx = {}
  sym_path_idx = [0] * len(syms)
  prev_obj = prev_src = prev_comp = None
  p_idx = 0
  for idx, s in enumerate(syms):
    obj, src, comp = s.object_path, s.source_path, s.component
    if obj is not prev_obj or src is not prev_src or comp is not prev_comp:
      prev_obj, prev_src, prev_comp = obj, src, comp
      p_idx = triple_to_idx.get((obj, src, comp))
      if p_idx is None:
        p_idx = len(triples)
        triple_to_idx[(obj, src, comp)] = p_idx
        triples.append((obj, src, comp))
        if comp not in comp_to_idx:
          comp_to_idx[comp] = len(comps)
          comps.append(comp)
    sym_path_idx[idx] = p_idx

  comp_indices = array.array('I', (comp_to_idx[c] for _, _, c in triples))
  strs = comps + [p for obj, src, _ in triples for p in (obj, src)]
  paths_bytes = (
    struct.pack('<II', len(comps), len(triples))
    + _ByteStreamSplit(comp_indices)
    + (
      '\x00'.join(strs).encode('utf-8', errors='surrogatepass') + b'\x00'
      if strs
      else b''
    )
  )
  out = [('paths', paths_bytes)]
  if sym_path_idx:
    out.append(('path_indices', _SplitZigZagDeltas(sym_path_idx)))
  return _CompressNamedBlobs(out)


def _BuildNumericBlobs(syms, has_padding):
  """Builds byte-split addresses, sizes, optional paddings, and flags blobs."""
  addrs = [s.address & 0xFFFFFFFF for s in syms]
  addr_deltas = array.array('I', [addrs[0]])
  addr_deltas.extend((b - a) & 0xFFFFFFFF for a, b in itertools.pairwise(addrs))
  sizes = array.array(
    'i',
    (
      s.size if s.full_name[:10] == 'Overhead: ' else s.size - s.padding
      for s in syms
    ),
  )
  flags = array.array('I', (s.flags for s in syms))
  out = [
    ('addresses', _ByteStreamSplit(addr_deltas)),
    ('sizes', _ByteStreamSplit(sizes)),
  ]
  if has_padding:
    paddings = array.array('i', (s.padding for s in syms))
    out.append(('paddings', _ByteStreamSplit(paddings)))
  out.append(('flags', _ByteStreamSplit(flags)))
  return _CompressNamedBlobs(out)


def _RunTasks(tasks):
  """Runs |tasks| across forked workers when single-threaded, else in-line."""
  if len(tasks) <= 2 or threading.active_count() > 1 or not hasattr(os, 'fork'):
    return [fn() for fn in tasks]
  with contextlib.ExitStack() as stack:
    files = [stack.enter_context(tempfile.TemporaryFile()) for _ in tasks]
    pids = []
    for fn, f in zip(tasks, files):
      pid = os.fork()
      if pid == 0:
        try:
          pickle.dump(fn(), f, protocol=pickle.HIGHEST_PROTOCOL)
          f.flush()
          os._exit(0)
        except BaseException:
          traceback.print_exc()
          os._exit(1)
      pids.append(pid)
    if any(os.waitpid(p, 0)[1] != 0 for p in pids):
      raise RuntimeError('Parallel v2 save worker failed')
    return [pickle.load(f) for f in files if f.seek(0) == 0]


def SaveSizeInfo(size_info, file_obj):
  """Serializes |size_info| into |file_obj| using .size v2 format."""
  syms = size_info.raw_symbols._symbols
  num_containers = len(size_info.containers)
  assert num_containers > 0, 'All containers were filtered out.'
  assert len(set(c.name for c in size_info.containers)) == num_containers, (
    'Container names must be distinct.'
  )
  total_symbols = len(syms)
  has_padding = bool(size_info.is_sparse)
  num_chunks = min(
    8, (total_symbols + _NAME_SHARD_SIZE - 1) // _NAME_SHARD_SIZE
  )
  chunk_size = (
    (total_symbols + num_chunks - 1) // num_chunks if num_chunks else 0
  )

  tasks = [
    lambda: _BuildMetaAndExtras(size_info, syms),
    lambda: _BuildPathsBlobs(syms),
  ]
  if total_symbols > 0:
    tasks.append(lambda: _BuildNumericBlobs(syms, has_padding))
    for k in range(num_chunks):
      s = k * chunk_size
      e = min(total_symbols, s + chunk_size)
      tasks.append(lambda k=k, s=s, e=e: [_EncodeNameShard(syms, k, s, e)])

  blobs = [entry for group in _RunTasks(tasks) for entry in group]
  header_flags = _FLAG_HAS_PADDING if has_padding else 0
  file_obj.write(
    V2_HEADER_PREFIX + struct.pack('<II', header_flags, len(blobs))
  )
  offset = 64 + _TOC_ENTRY_STRUCT.size * len(blobs)
  for name, raw_len, comp in blobs:
    file_obj.write(
      _TOC_ENTRY_STRUCT.pack(
        name.encode('ascii').ljust(16, b'\x00'), offset, len(comp), raw_len
      )
    )
    offset += len(comp)
  for _, _, comp in blobs:
    file_obj.write(comp)


def LoadSizeInfo(file_obj, size_path, is_sparse=False):
  """Loads a .size v2 SizeInfo from |file_obj| (after V2_HEADER_PREFIX)."""
  header_flags, num_blobs = struct.unpack('<II', file_obj.read(8))
  has_padding = bool(header_flags & _FLAG_HAS_PADDING)
  toc_bytes = file_obj.read(_TOC_ENTRY_STRUCT.size * num_blobs)
  cur_pos = 64 + _TOC_ENTRY_STRUCT.size * num_blobs
  names = []
  comp_chunks = []
  for name_bytes, offset, comp_size, _ in _TOC_ENTRY_STRUCT.iter_unpack(
    toc_bytes
  ):
    if offset > cur_pos:
      file_obj.read(offset - cur_pos)
    names.append(name_bytes.rstrip(b'\x00').decode('ascii'))
    comp_chunks.append(file_obj.read(comp_size))
    cur_pos = offset + comp_size

  max_workers = min(16, os.cpu_count() or 4, max(1, num_blobs))
  with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as ex:
    blobs = dict(zip(names, ex.map(zstd.decompress, comp_chunks)))

  meta = json.loads(blobs['meta'])
  build_config = meta['build_config']
  containers = [
    models.Container(
      name=c['name'],
      metadata=c['metadata'],
      section_sizes=c['section_sizes'],
      metrics_by_file=c.get('metrics_by_file', {}),
    )
    for c in meta['containers']
  ]
  models.BaseContainer.AssignShortNames(containers)
  total_symbols = meta['total_symbols']
  if total_symbols == 0:
    return models.SizeInfo(
      build_config, containers, [], size_path=size_path, is_sparse=is_sparse
    )

  path_data = blobs['paths']
  num_comps, num_paths = struct.unpack_from('<II', path_data, 0)
  if num_paths > 0:
    comp_end = 8 + num_paths * 4
    comp_idx = _ByteStreamUnsplit(path_data[8:comp_end], 'I', num_paths)
    strs = (
      path_data[comp_end:-1]
      .decode('utf-8', errors='surrogatepass')
      .split('\x00')
    )
    path_tuples = [
      (strs[num_comps + 2 * i], strs[num_comps + 2 * i + 1], strs[comp_idx[i]])
      for i in range(num_paths)
    ]
  else:
    path_tuples = []

  addresses = itertools.accumulate(
    _ByteStreamUnsplit(blobs['addresses'], 'I', total_symbols)
  )
  sizes = _ByteStreamUnsplit(blobs['sizes'], 'i', total_symbols)
  paddings = (
    _ByteStreamUnsplit(blobs['paddings'], 'i', total_symbols)
    if has_padding
    else itertools.repeat(0)
  )
  path_indices = _UnsplitZigZagDeltas(blobs['path_indices'], total_symbols)
  flags = _ByteStreamUnsplit(blobs['flags'], 'I', total_symbols)

  raw_symbols = []
  append_sym = raw_symbols.append
  symbol_cls = models.Symbol
  symbol_new = symbol_cls.__new__
  sym_it = zip(addresses, sizes, path_indices, flags, paddings)
  for c_idx, sec_name, sym_count in meta['segments']:
    cur_container = containers[c_idx]
    for addr, sz, p_idx, flg, pad in itertools.islice(sym_it, sym_count):
      new_sym = symbol_new(symbol_cls)
      new_sym.container = cur_container
      new_sym.section_name = sec_name
      new_sym.address = addr & 0xFFFFFFFF
      new_sym.size = sz
      new_sym.object_path, new_sym.source_path, new_sym.component = path_tuples[
        p_idx
      ]
      new_sym.flags = flg
      new_sym.disassembly = ''
      new_sym.aliases = None
      new_sym.template_name = ''
      new_sym.name = ''
      new_sym.padding = pad
      append_sym(new_sym)

  string_literal_name = models.STRING_LITERAL_NAME
  for k in itertools.count():
    ndata = blobs.get(f'names_{k}')
    if ndata is None:
      break
    start_idx, count = struct.unpack_from('<II', ndata, 0)
    perm_end = 8 + count * 4
    lcp_end = perm_end + count
    lcp_arr = ndata[perm_end:lcp_end]
    suffixes = ndata[lcp_end:-1].split(b'\x00')
    perm = _UnsplitZigZagDeltas(ndata[8:perm_end], count)
    prev = b''
    for rank, lcp, suf in zip(perm, lcp_arr, suffixes):
      prev = prev[:lcp] + suf
      full_name = prev.decode('utf-8', errors='surrogatepass')
      raw_symbols[start_idx + rank].full_name = (
        string_literal_name if full_name == string_literal_name else full_name
      )

  if has_padding:
    for s in raw_symbols:
      if s.full_name[:10] != 'Overhead: ':
        s.size += s.padding

  adata = blobs.get('aliases')
  if adata is not None:
    num_groups = struct.unpack_from('<I', adata, 0)[0]
    first_deltas = _ByteStreamUnsplit(
      adata[4 : 4 + num_groups * 4], 'I', num_groups
    )
    counts = _ByteStreamUnsplit(adata[4 + num_groups * 4 :], 'I', num_groups)
    first_idx = 0
    for d, cnt in zip(first_deltas, counts):
      first_idx += d
      group = raw_symbols[first_idx : first_idx + cnt]
      for s in group:
        s.aliases = group

  if not has_padding:
    file_format.CalculatePadding(raw_symbols)

  ddata = blobs.get('disassembly')
  if ddata is not None:
    d_count = struct.unpack_from('<I', ddata, 0)[0]
    d_pos = 4
    for _ in range(d_count):
      s_idx, byte_len = struct.unpack_from('<II', ddata, d_pos)
      d_pos += 8
      raw_symbols[s_idx].disassembly = ddata[d_pos : d_pos + byte_len].decode(
        'utf-8', errors='surrogatepass'
      )
      d_pos += byte_len

  return models.SizeInfo(
    build_config,
    containers,
    raw_symbols,
    size_path=size_path,
    is_sparse=is_sparse,
  )
