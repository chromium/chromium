#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Rebuilds PNG files intended for use as icon images on the Mac. It is
# reasonably robust, but is not designed to be robust against adversarially-
# constructed files.
#
# This is an opinionated script and makes assumptions about the desired
# characteristics of a PNG file for use as a Mac icon. All users of this script
# must verify that those are the correct assumptions for their use case before
# using it.

import argparse
import binascii
import os
import struct
import sys

_PNG_MAGIC = b'\x89\x50\x4e\x47\x0d\x0a\x1a\x0a'
_CHUNK_HEADER_STRUCT = struct.Struct('>I4s')
_CHUNK_CRC_STRUCT = struct.Struct('>I')


class FormatError(Exception):
    pass


def _process_path(path):
    with open(path, 'r+b') as file:
        return _process_file(file)


def _process_file(file):
    magic = file.read(len(_PNG_MAGIC))
    if magic != _PNG_MAGIC:
        raise FormatError(file, file.tell(), 'bad magic', magic, _PNG_MAGIC)

    chunks = {}

    while True:
        chunk_header = file.read(_CHUNK_HEADER_STRUCT.size)
        (chunk_length, chunk_type) = _CHUNK_HEADER_STRUCT.unpack(chunk_header)
        chunk = chunk_header + file.read(chunk_length + _CHUNK_CRC_STRUCT.size)

        if chunk_type in chunks:
            raise FormatError(file, file.tell(), 'duplicate chunk', chunk_type)
        chunks[chunk_type] = chunk

        if chunk_type == b'IEND':
            break

    eof = file.read(1)
    if len(eof) != 0:
        raise FormatError(file, '\'IEND\' chunk not at end')

    ihdr = chunks[b'IHDR'][_CHUNK_HEADER_STRUCT.size:-_CHUNK_CRC_STRUCT.size]
    (ihdr_width, ihdr_height, ihdr_bit_depth, ihdr_color_type,
     ihdr_compression_method, ihdr_filter_method,
     ihdr_interlace_method) = struct.unpack('>2I5b', ihdr)

    # The only two color types that have transparency and can be used for icons
    # are types 3 (indexed) and 6 (direct RGBA).
    if ihdr_color_type not in (3, 6):
        raise FormatError(file, 'disallowed color type', ihdr_color_type)
    if ihdr_color_type == 3 and b'PLTE' not in chunks:
        raise FormatError(file, 'indexed color requires \'PLTE\' chunk')
    if ihdr_color_type == 3 and b'tRNS' not in chunks:
        raise FormatError(file, 'indexed color requires \'tRNS\' chunk')

    if b'IDAT' not in chunks:
        raise FormatError(file, 'missing \'IDAT\' chunk')

    if b'iCCP' in chunks:
        raise FormatError(file, 'disallowed color profile; sRGB only')

    if b'sRGB' not in chunks:
        # Note that a value of 0 is a perceptual rendering intent (e.g.
        # photographs) while a value of 1 is a relative colorimetric rendering
        # intent (e.g. icons). Every macOS icon that has an 'sRGB' chunk uses 0
        # so that is what is used here. Please forgive us, UX.
        #
        # Reference:
        #   http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.sRGB
        srgb_chunk_type = struct.pack('>4s', b'sRGB')
        srgb_chunk_data = struct.pack('>b', 0)  # Perceptual
        srgb_chunk_length = struct.pack('>I', len(srgb_chunk_data))
        srgb_chunk_crc = struct.pack(
            '>I', binascii.crc32(srgb_chunk_type + srgb_chunk_data))
        chunks[b'sRGB'] = (
            srgb_chunk_length + srgb_chunk_type + srgb_chunk_data +
            srgb_chunk_crc)

    file.seek(len(_PNG_MAGIC), os.SEEK_SET)

    file.write(chunks[b'IHDR'])
    file.write(chunks[b'sRGB'])
    if ihdr_color_type == 3:
        file.write(chunks[b'PLTE'])
        file.write(chunks[b'tRNS'])
    file.write(chunks[b'IDAT'])
    file.write(chunks[b'IEND'])

    file.truncate()


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('paths', nargs='+', metavar='path')
    parsed = parser.parse_args(args)

    for path in parsed.paths:
        print(path)
        _process_path(path)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
