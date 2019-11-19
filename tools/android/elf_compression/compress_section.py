#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script compresses a part of shared library without breaking it.

It compresses the specified file range which will be used by a library's
decompression hook that uses userfaultfd to intercept access attempts to the
range and decompress its data on demand.

Technically this script does the following steps:
  1) Makes a copy of specified range, compresses it and adds it to the binary
    as a new section using objcopy.
  2) Moves the Phdr section to the end of the file to ease next manipulations on
    it.
  3) Creates a LOAD segment for the compressed section created at step 1.
  4) Splits the LOAD segments so the range lives in its own
    LOAD segment.
  5) Cuts the range out of the binary, correcting offsets, broken in the
    process.
  6) Changes cut range LOAD segment by zeroing the file_sz and setting
    mem_sz to the original range size, essentially lazily zeroing the space.
  7) Changes magic bytes provided by the decompression hook to contain
   addresses of cut range and compressed range.
"""

import argparse
import logging
import os
import pathlib
import subprocess
import sys
import tempfile

import compression
import elf_headers

COMPRESSED_SECTION_NAME = '.compressed_library_data'
ADDRESS_ALIGN = 0x1000

CUT_RANGE_BEGIN_MAGIC = bytearray(
    [0x2e, 0x2a, 0xee, 0xf6, 0x45, 0x03, 0xd2, 0x50])
CUT_RANGE_END_MAGIC = bytearray(
    [0x52, 0x40, 0xeb, 0x9d, 0xdb, 0x11, 0xed, 0x1a])
COMPRESSED_RANGE_BEGIN_MAGIC = bytearray(
    [0x5e, 0x49, 0x4a, 0x4c, 0xae, 0x28, 0xc8, 0xbb])
COMPRESSED_RANGE_END_MAGIC = bytearray(
    [0xdd, 0x60, 0xed, 0xcf, 0xc3, 0x29, 0xa6, 0xd6])

# src/third_party/llvm-build/Release+Asserts/bin/llvm-objcopy
OBJCOPY_PATH = pathlib.Path(__file__).resolve().parents[3].joinpath(
    'third_party/llvm-build/Release+Asserts/bin/llvm-objcopy')


def SegmentContains(main_l, main_r, l, r):
  """Returns true if [l, r) is contained inside [main_l, main_r).

  Args:
    main_l: int. Left border of the first segment.
    main_r: int. Right border (exclusive) of the second segment.
    l: int. Left border of the second segment.
    r: int. Right border (exclusive) of the second segment.
  """
  return main_l <= l and main_r >= r


def SegmentsIntersect(l1, r1, l2, r2):
  """Returns true if [l1, r1) intersects with [l2, r2).

  Args:
    l1: int. Left border of the first segment.
    r1: int. Right border (exclusive) of the second segment.
    l2: int. Left border of the second segment.
    r2: int. Right border (exclusive) of the second segment.
  """
  return l2 < r1 and r2 > l1


def AlignUp(addr, page_size=ADDRESS_ALIGN):
  """Rounds up given address to be aligned to page_size.

  Args:
    addr: int. Virtual address to be aligned.
    page_size: int. Page size to be used for the alignment.
  """
  if addr % page_size != 0:
    addr += page_size - (addr % page_size)
  return addr


def AlignDown(addr, page_size=ADDRESS_ALIGN):
  """Round down given address to be aligned to page_size.

  Args:
    addr: int. Virtual address to be aligned.
    page_size: int. Page size to be used for the alignment.
  """
  return addr - addr % page_size


def MatchVaddrAlignment(vaddr, offset, align=ADDRESS_ALIGN):
  """Align vaddr to comply with ELF standard binary alignment.

  Increases vaddr until the following is true:
    vaddr % align == offset % align

  Args:
    vaddr: virtual address to be aligned.
    offset: file offset to be aligned.
    align: alignment value.

  Returns:
    Aligned virtual address, bigger or equal than the vaddr.
  """
  delta = offset % align - vaddr % align
  if delta < 0:
    delta += align
  return vaddr + delta


def _SetupLogging():
  logging.basicConfig(
      format='%(asctime)s %(filename)s:%(lineno)s %(levelname)s] %(message)s',
      datefmt='%H:%M:%S',
      level=logging.ERROR)


def _ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-i', '--input', help='Shared library to parse', required=True)
  parser.add_argument(
      '-o', '--output', help='Name of the output file', required=True)
  parser.add_argument(
      '-l',
      '--left_range',
      help='Beginning of the target part of the library',
      type=int,
      required=True)
  parser.add_argument(
      '-r',
      '--right_range',
      help='End (exclusive) of the target part of the library',
      type=int,
      required=True)
  return parser.parse_args()


def _FileRangeToVirtualAddressRange(data, l, r):
  """Returns virtual address range corresponding to given file range.

  Since we have to resolve them by their virtual address, parsing of LOAD
  segments is required here.
  """
  elf = elf_headers.ElfHeader(data)
  for phdr in elf.GetProgramHeadersByType(
      elf_headers.ProgramHeader.Type.PT_LOAD):
    # Current version of the prototype only supports ranges which are fully
    # contained inside one LOAD segment. It should cover most of the common
    # cases.
    if not SegmentsIntersect(phdr.p_offset, phdr.FilePositionEnd(), l, r):
      continue
    if not SegmentContains(phdr.p_offset, phdr.FilePositionEnd(), l, r):
      raise RuntimeError('Range is not contained within one LOAD segment')
    l_virt = phdr.p_vaddr + (l - phdr.p_offset)
    r_virt = phdr.p_vaddr + (r - phdr.p_offset)
    return l_virt, r_virt
  raise RuntimeError('Specified range is outside of all LOAD segments.')


def _CopyRangeIntoCompressedSection(data, l, r):
  """Adds a new section containing compressed version of provided range."""
  compressed_range = compression.CompressData(data[l:r])

  with tempfile.TemporaryDirectory() as tmpdir:
    # The easiest way to add a new section is to use objcopy, but it requires
    # for all of the data to be stored in files.
    objcopy_input_file = os.path.join(tmpdir, 'input')
    objcopy_data_file = os.path.join(tmpdir, 'data')
    objcopy_output_file = os.path.join(tmpdir, 'output')

    with open(objcopy_input_file, 'wb') as f:
      f.write(data)
    with open(objcopy_data_file, 'wb') as f:
      f.write(compressed_range)

    objcopy_args = [
        OBJCOPY_PATH, objcopy_input_file, objcopy_output_file, '--add-section',
        '{}={}'.format(COMPRESSED_SECTION_NAME, objcopy_data_file)
    ]
    run_result = subprocess.run(
        objcopy_args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if run_result.returncode != 0:
      raise RuntimeError('objcopy failed with status code {}: {}'.format(
          run_result.returncode, run_result.stderr))

    with open(objcopy_output_file, 'rb') as f:
      data[:] = bytearray(f.read())


def _FindNewVaddr(phdrs):
  """Returns the virt address that is safe to use for insertion of new data."""
  max_vaddr = 0
  # Strictly speaking it should be sufficient to look through only LOAD
  # segments, but better be safe than sorry.
  for phdr in phdrs:
    max_vaddr = max(max_vaddr, phdr.p_vaddr + phdr.p_memsz)
  # When the mapping occurs end address is increased to be a multiple
  # of page size. To ensure compatibility with anything relying on this
  # behaviour we take this increase into account.
  max_vaddr = AlignUp(max_vaddr)
  return max_vaddr


def _MovePhdrToTheEnd(data):
  """Moves Phdrs to the end of the file and adjusts all references to it."""
  elf_hdr = elf_headers.ElfHeader(data)

  # If program headers are already in the end of the file, nothing to do.
  if elf_hdr.e_phoff + elf_hdr.e_phnum * elf_hdr.e_phentsize == len(data):
    return

  old_phoff = elf_hdr.e_phoff
  new_phoff = elf_hdr.e_phoff = len(data)

  unaligned_new_vaddr = _FindNewVaddr(elf_hdr.GetProgramHeaders())
  new_vaddr = MatchVaddrAlignment(unaligned_new_vaddr, new_phoff)
  # Since we moved the PHDR section to the end of the file, we need to create a
  # new LOAD segment to load it in.
  current_filesize = elf_hdr.e_phnum * elf_hdr.e_phentsize
  # We are using current_filesize while adding new program header due to
  # AddProgramHeader handling the increase of size due to addition of new
  # header.
  elf_hdr.AddProgramHeader(
      elf_headers.ProgramHeader.Create(
          elf_hdr.byte_order,
          p_type=elf_headers.ProgramHeader.Type.PT_LOAD,
          p_flags=elf_headers.ProgramHeader.Flags.PF_R,
          p_offset=new_phoff,
          p_vaddr=new_vaddr,
          p_paddr=new_vaddr,
          p_filesz=current_filesize,
          p_memsz=current_filesize,
          p_align=ADDRESS_ALIGN,
      ))

  # PHDR segment if it exists should point to the new location.
  for phdr in elf_hdr.GetProgramHeadersByType(
      elf_headers.ProgramHeader.Type.PT_PHDR):
    phdr.p_offset = new_phoff
    phdr.p_vaddr = new_vaddr
    phdr.p_paddr = new_vaddr
    phdr.p_align = ADDRESS_ALIGN

  # We need to replace the previous phdr placement with zero bytes to fail
  # fast if dynamic linker doesn't like the new program header.
  previous_phdr_size = (elf_hdr.e_phnum - 1) * elf_hdr.e_phentsize
  data[old_phoff:old_phoff + previous_phdr_size] = [0] * previous_phdr_size

  # Updating ELF header to point to the new location.
  elf_hdr.PatchData(data)


def _CreateLoadForCompressedSection(data):
  """Creates a LOAD segment to previously created COMPRESSED_SECTION_NAME.

  Returns the virtual address range corresponding to created segment."""
  elf_hdr = elf_headers.ElfHeader(data)

  section_offset = None
  section_size = None
  for shdr in elf_hdr.GetSectionHeaders():
    if shdr.GetStrName() == COMPRESSED_SECTION_NAME:
      section_offset = shdr.sh_offset
      section_size = shdr.sh_size
      break
  if section_offset is None:
    raise RuntimeError(
        'Failed to locate {} section in file'.format(COMPRESSED_SECTION_NAME))

  unaligned_new_vaddr = _FindNewVaddr(elf_hdr.GetProgramHeaders())
  new_vaddr = MatchVaddrAlignment(unaligned_new_vaddr, section_offset)
  elf_hdr.AddProgramHeader(
      elf_headers.ProgramHeader.Create(
          elf_hdr.byte_order,
          p_type=elf_headers.ProgramHeader.Type.PT_LOAD,
          p_flags=elf_headers.ProgramHeader.Flags.PF_R,
          p_offset=section_offset,
          p_vaddr=new_vaddr,
          p_paddr=new_vaddr,
          p_filesz=section_size,
          p_memsz=section_size,
          p_align=ADDRESS_ALIGN,
      ))
  elf_hdr.PatchData(data)
  return new_vaddr, new_vaddr + section_size


def _SplitLoadSegmentAndNullifyRange(data, l, r):
  """Find LOAD segment covering [l, r) and splits it into three segments.

  Split is done so one of the LOAD segments contains only [l, r) and nothing
  else. If the range is located at the start or at the end of the segment less
  than three segments may be created.

  The resulting LOAD segment containing [l, r) is edited so it sets the
  corresponding virtual address range to zeroes, ignoring file content.

  Returns virtual address range corresponding to [l, r).
  """
  elf_hdr = elf_headers.ElfHeader(data)

  range_phdr = None
  for phdr in elf_hdr.GetProgramHeadersByType(
      elf_headers.ProgramHeader.Type.PT_LOAD):
    if SegmentContains(phdr.p_offset, phdr.FilePositionEnd(), l, r):
      range_phdr = phdr
      break
  if range_phdr is None:
    raise RuntimeError('No LOAD segment covering the range found')

  # The range_phdr will become the LOAD segment containing the [l, r) range
  # but we need to create the additional two segments.
  left_segment_size = l - range_phdr.p_offset
  if left_segment_size > 0:
    # Creating LOAD segment containing the [phdr.p_offset, l) part.
    elf_hdr.AddProgramHeader(
        elf_headers.ProgramHeader.Create(
            elf_hdr.byte_order,
            p_type=range_phdr.p_type,
            p_flags=range_phdr.p_flags,
            p_offset=range_phdr.p_offset,
            p_vaddr=range_phdr.p_vaddr,
            p_paddr=range_phdr.p_paddr,
            p_filesz=left_segment_size,
            p_memsz=left_segment_size,
            p_align=range_phdr.p_align,
        ))
  if range_phdr.p_offset + range_phdr.p_memsz > r:
    # Creating LOAD segment containing the [r, phdr.p_offset + phdr.p_memsz).
    right_segment_delta = r - range_phdr.p_offset
    right_segment_address = range_phdr.p_vaddr + right_segment_delta
    right_segment_filesize = max(range_phdr.p_filesz - right_segment_delta, 0)
    right_segment_memsize = range_phdr.p_memsz - right_segment_delta
    elf_hdr.AddProgramHeader(
        elf_headers.ProgramHeader.Create(
            elf_hdr.byte_order,
            p_type=range_phdr.p_type,
            p_flags=range_phdr.p_flags,
            p_offset=r,
            p_vaddr=right_segment_address,
            p_paddr=right_segment_address,
            p_filesz=right_segment_filesize,
            p_memsz=right_segment_memsize,
            p_align=range_phdr.p_align,
        ))
  # Modifying the range_phdr
  central_segment_address = range_phdr.p_vaddr + left_segment_size
  range_phdr.p_offset = l
  range_phdr.p_vaddr = central_segment_address
  range_phdr.p_paddr = central_segment_address
  range_phdr.p_filesz = 0
  range_phdr.p_memsz = r - l

  elf_hdr.PatchData(data)
  return central_segment_address, central_segment_address + (r - l)


def _CutRangeAndCorrectFile(data, l, r):
  """Removes [l, r) from the data and fixes offsets to stabilize the ELF."""
  elf = elf_headers.ElfHeader(data)
  # Removing the range from the file:
  del data[l:r]

  range_length = r - l
  for phdr in elf.GetProgramHeaders():
    # Any other program header intersecting the [l, r) range poses serious
    # problem as this header needs to be split if possible. However since we are
    # compressing part of program's code this is highly unlikely, albeit
    # possible in worst case scenario.
    # With that in mind we assert that such thing doesn't happen in our case.
    if SegmentsIntersect(phdr.p_offset, phdr.FilePositionEnd(), l, r):
      raise RuntimeError('Segment intersects with provided range')
    if phdr.p_offset >= r:
      phdr.p_offset -= range_length
    # Per ELF standard: p_offset % p_align == p_vaddr % p_align.
    # Since we moved the p_offset we could have broken this rule.
    # To mitigate this issue we notice the following two facts:
    # 1) range_length % ADDRESS_ALIGN == 0.
    # 2) We can reduce the p_align without breaking the alignment.
    # We reduce all p_align to be less or equal than ADDRESS_ALIGN and now
    # range_length % p_align == 0, so the alignment remains valid.
    # Our changes of p_align are perfectly legal per standard
    # as long as p_align % PAGE_SIZE == 0.
    if phdr.p_align > ADDRESS_ALIGN:
      phdr.p_align = ADDRESS_ALIGN

  for shdr in elf.GetSectionHeaders():
    # Note that if the section overlaps with the cut range we are unable
    # to adjust its size to match both file and virtual size of it. In such
    # case we treat sh_size as memory size and don't adjust it, which may
    # cause some tools treating sh_size as file size to stop working.
    if shdr.sh_offset >= l:
      if shdr.sh_offset < r:
        raise RuntimeError('Section starts within the provided range')
      else:
        shdr.sh_offset -= range_length
  if elf.e_phoff > l:
    elf.e_phoff -= range_length
  if elf.e_shoff > l:
    elf.e_shoff -= range_length
  elf.PatchData(data)


def _PatchConstructorBytes(data, cut_range_virt_l, cut_range_virt_r,
                           compressed_range_virt_l, compressed_range_virt_r):
  """Sets magic bytes given by constructor to the given ranges."""
  elf = elf_headers.ElfHeader(data)

  to_patch = [
      (CUT_RANGE_BEGIN_MAGIC, cut_range_virt_l, 'cut range begin'),
      (CUT_RANGE_END_MAGIC, cut_range_virt_r, 'cut range end'),
      (COMPRESSED_RANGE_BEGIN_MAGIC, compressed_range_virt_l,
       'compressed range begin'),
      (COMPRESSED_RANGE_END_MAGIC, compressed_range_virt_r,
       'compressed range end'),
  ]

  for magic_bytes, new_value, name in to_patch:
    magic_idx = data.find(magic_bytes)
    if magic_idx == -1:
      raise RuntimeError('failed to find %s magic bytes' % name)
    if data.rfind(magic_bytes) != magic_idx:
      raise RuntimeError('%s magic bytes occures more then once' % name)
    new_value_bytes = new_value.to_bytes(length=8, byteorder=elf.byte_order)
    data[magic_idx:magic_idx + 8] = new_value_bytes


def _ShrinkRangeToAlignVirtualAddress(data, l, r):
  virtual_l, virtual_r = _FileRangeToVirtualAddressRange(data, l, r)
  # LOAD segments borders are being rounded to the page size so we have to
  # shrink [l, r) so corresponding virtual addresses are aligned.
  l += AlignUp(virtual_l) - virtual_l
  r -= virtual_r - AlignDown(virtual_r)
  return l, r


def main():
  _SetupLogging()
  args = _ParseArguments()

  with open(args.input, 'rb') as f:
    data = f.read()
    data = bytearray(data)

  left_range, right_range = _ShrinkRangeToAlignVirtualAddress(
      data, args.left_range, args.right_range)
  if left_range >= right_range:
    raise RuntimeError('Range collapsed after aligning by page size')

  _CopyRangeIntoCompressedSection(data, left_range, right_range)
  _MovePhdrToTheEnd(data)

  compressed_virt_l, compressed_virt_r = _CreateLoadForCompressedSection(data)
  virt_l, virt_r = _SplitLoadSegmentAndNullifyRange(data, left_range,
                                                    right_range)
  _CutRangeAndCorrectFile(data, left_range, right_range)
  _PatchConstructorBytes(data, virt_l, virt_r, compressed_virt_l,
                         compressed_virt_r)

  with open(args.output, 'wb') as f:
    f.write(data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
