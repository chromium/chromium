#!/usr/bin/env python

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# maketoc inserts a table of contents (TOC) into an icns file if one does not
# exist, or verifies it if it is present. The icns files produced by makeicns
# do not contain TOCs. This script will add them.
#
# TOCs are not required, but are produced by recent icns-writing tools like
# icnsutil.

import struct
import sys


def _interpret_header(header):
    assert len(header) == 8
    (magic, length) = struct.unpack('>4sI', header)
    assert length >= len(header)
    return (magic, length)


def _read_header(file):
    return _interpret_header(file.read(8))


def _write_header(file, magic, length):
    header = struct.pack('>4sI', magic, length)
    file.write(header)


def make_toc(path):
    file = open(path, 'r+b')
    (file_magic, file_length) = _read_header(file)
    assert file_magic == b'icns'

    read_length = file.tell()
    chunk_list = []
    chunk_dict = {}

    for chunk_header in iter(lambda: file.read(8), b''):
        (chunk_magic, chunk_length) = _interpret_header(chunk_header)

        assert chunk_magic not in chunk_dict
        chunk_list.append(chunk_magic)
        chunk_dict[chunk_magic] = chunk_length
        read_length += chunk_length
        file.seek(chunk_length - 8, 1)

    assert read_length == file_length

    if b'TOC ' in chunk_dict:
        # TOC exists, verify it.
        assert chunk_list[0] == b'TOC '

        toc_length = chunk_dict[b'TOC ']
        assert toc_length % 8 == 0

        chunk_count = (toc_length - 8) // 8
        assert chunk_count == len(chunk_list) - 1

        # The TOC just duplicates each chunk's header in sequence except for
        # its own.
        file.seek(16)
        for i in range(0, chunk_count):
            (chunk_magic, chunk_length) = _read_header(file)
            assert chunk_magic == chunk_list[i + 1]
            assert chunk_length == chunk_dict[chunk_magic]
    else:
        # TOC is not present, create it.

        # Update the file header with the new length.
        toc_length = 8 + len(chunk_list) * 8
        file_length += toc_length
        file.seek(0)
        _write_header(file, b'icns', file_length)
        after_file_header = file.tell()

        remainder = file.read()
        file.seek(after_file_header)

        # The TOC just duplicates each chunk's header in sequence except for
        # its own.
        _write_header(file, b'TOC ', toc_length)
        for chunk_magic in chunk_list:
            _write_header(file, chunk_magic, chunk_dict[chunk_magic])

        file.write(remainder)

        assert file_length == file.tell()

    file.close()


def main(args):
    for path in args:
        print(path)
        make_toc(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
