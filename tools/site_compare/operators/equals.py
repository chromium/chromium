# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compare two images for equality."""

from PIL import Image
from PIL import ImageChops


def Compare(file1, file2, **kwargs):
  """Compares two images to see if they're identical.

  Args:
    file1: path to first image to compare
    file2: path to second image to compare
    kwargs: unused for this operator

  Returns:
    None if the images are identical
    A tuple of (errorstring, image) if they're not
  """
  kwargs = kwargs  # unused parameter

  im1 = Image.open(file1)
  im2 = Image.open(file2)

  if im1.size != im2.size:
    return ("The images are of different size (%s vs %s)" %
            (im1.size, im2.size), im1)

  diff = ImageChops.difference(im1, im2)

  if max(diff.getextrema()) != (0, 0):
    return ("The images differ", diff)
  else:
    return None
