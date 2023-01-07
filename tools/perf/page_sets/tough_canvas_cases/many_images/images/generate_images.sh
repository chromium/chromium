#!/bin/bash
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Utility to generate random images using ImageMagick.

MIN_DIM=75
MAX_DIM=100
NUM_IMAGES=400

for (( i = 1; i <= NUM_IMAGES; ++i )) do
  # Generate a random binary 0/1.
  rand_bin=$(( $RANDOM % 2 ))
  # Generate a random number in the range [MIN_DIM, MAX_DIM].
  rand_dim=$(( MIN_DIM + ($RANDOM % (MAX_DIM - MIN_DIM + 1)) ))

  # Generate a dimension such that one side is equal to MAX_DIM
  # and the other is a random number in the range [MIN_DIM, MAX_DIM].
  width=$(( (rand_bin * MAX_DIM) + ((1 - rand_bin) * rand_dim) ))
  height=$(( ((1 - rand_bin) * MAX_DIM) + (rand_bin * rand_dim) ))

  # Generate a random image
  convert -size ${width}x${height} plasma:fractal image${i}_t.png
done
