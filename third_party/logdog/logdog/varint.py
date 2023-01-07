# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import os
import struct
import sys


def write_uvarint(w, val):
  """Writes a varint value to the supplied file-like object.

  Args:
    w (object): A file-like object to write to. Must implement write.
    val (number): The value to write. Must be >= 0.

  Returns (int): The number of bytes that were written.

  Raises:
    ValueError if 'val' is < 0.
  """
  if val < 0:
    raise ValueError('Cannot encode negative value, %d' % (val,))

  count = 0
  while val > 0 or count == 0:
    byte = (val & 0b01111111)
    val >>= 7
    if val > 0:
      byte |= 0b10000000

    w.write(struct.pack('B', byte))
    count += 1
  return count


def read_uvarint(r):
  """Reads a uvarint from a stream.

  This is targeted towards testing, and will not be used in production code.

  Args:
    r (object): A file-like object to read from. Must implement read.

  Returns: (value, count)
    value (int): The decoded varint number.
    count (int): The number of bytes that were read from 'r'.

  Raises:
    ValueError if the encoded varint is not terminated.
  """
  count = 0
  result = 0
  while True:
    byte = r.read(1)
    if len(byte) == 0:
      raise ValueError('UVarint was not terminated')

    byte = struct.unpack('B', byte)[0]
    result |= ((byte & 0b01111111) << (7 * count))
    count += 1
    if byte & 0b10000000 == 0:
      break
  return result, count
