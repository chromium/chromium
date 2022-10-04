#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import division
from __future__ import print_function

from array import array
import os
import random
import shutil
import sys

MAX_INPUT_SIZE = 5000  # < 1 MB so we don't blow up fuzzer build sizes.
MAX_FLOAT32 = 3.4028235e+38


def IsValidSize(n):
  if n == 0:
    return False
  # PFFFT only supports transforms for inputs of length N of the form
  # N = (2^a)*(3^b)*(5^c) where a >= 5, b >=0, c >= 0.
  FACTORS = [2, 3, 5]
  factorization = [0, 0, 0]
  for i, factor in enumerate(FACTORS):
    while n % factor == 0:
      n = n // factor
      factorization[i] += 1
  return factorization[0] >= 5 and n == 1


def WriteFloat32ArrayToFile(file_path, size, generator):
  """Generate an array of float32 values and writes to file."""
  with open(file_path, 'wb') as f:
    float_array = array('f', [generator() for _ in range(size)])
    float_array.tofile(f)


def main():
  if len(sys.argv) < 2:
    print('Usage: %s <path to output directory>' % sys.argv[0])
    sys.exit(1)

  output_path = sys.argv[1]
  # Start with a clean output directory.
  if os.path.exists(output_path):
    shutil.rmtree(output_path)
  os.makedirs(output_path)

  # List of valid input sizes.
  N = [n for n in range(MAX_INPUT_SIZE) if IsValidSize(n)]

  # Set the seed to always generate the same random data.
  random.seed(0)

  # Generate different types of input arrays for each target length.
  for n in N:
    # Zeros.
    WriteFloat32ArrayToFile(
        os.path.join(output_path, 'zeros_%d' % n), n, lambda: 0)
    # Max float 32.
    WriteFloat32ArrayToFile(
        os.path.join(output_path, 'max_%d' % n), n, lambda: MAX_FLOAT32)
    # Random values in the s16 range.
    rnd_s16 = lambda: 32768.0 * 2.0 * (random.random() - 0.5)
    WriteFloat32ArrayToFile(
        os.path.join(output_path, 'rnd_s16_%d' % n), n, rnd_s16)

  sys.exit(0)


if __name__ == '__main__':
  main()
