#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Does a quick parse of a png file, to make sure that no unneeded png chunks are
# present in the file.

import struct
import sys


def verify(path):
    file = open(path, 'rb')
    magic = file.read(8)
    assert magic == b'\x89\x50\x4e\x47\x0d\x0a\x1a\x0a'

    color_type = None
    chunk_types = []
    while True:
        chunk_header = file.read(8)
        (chunk_length, chunk_type) = struct.unpack('>I4s', chunk_header)

        if chunk_type == b'IHDR':
            ihdr = file.read(chunk_length)
            (width, height, bit_depth, color_type, compression_method,
             filter_method, interlace_method) = struct.unpack('>2I5b', ihdr)
        else:
            file.seek(chunk_length, 1)

        chunk_footer = file.read(4)
        (chunk_crc,) = struct.unpack('>I', chunk_footer)

        chunk_types.append(chunk_type)

        if chunk_type == b'IEND':
            break

    # The only two color types that have transparency and can be used for icons
    # are types 3 (indexed) and 6 (direct RGBA).
    assert color_type in (3, 6), "Disallowed color type {}".format(color_type)
    allowed_chunk_types = [b'IHDR', b'sRGB', b'IDAT', b'IEND']
    if color_type == 3:
        allowed_chunk_types.extend([b'PLTE', b'tRNS'])

    for chunk_type in chunk_types:
        assert (chunk_type in allowed_chunk_types
               ), "Disallowed chunk of type {}".format(chunk_type)

    eof = file.read(1)
    assert len(eof) == 0

    file.close()


def main(args):
    for path in args:
        print(path)
        verify(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
