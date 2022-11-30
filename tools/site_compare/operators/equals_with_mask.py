# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compare two images for equality, subject to a mask."""

from PIL import Image
from PIL import ImageChops

import os.path


def Compare(file1, file2, **kwargs):
  """Compares two images to see if they're identical subject to a mask.

  An optional directory containing masks is supplied. If a mask exists
  which matches file1's name, areas under the mask where it's black
  are ignored.

  Args:
    file1: path to first image to compare
    file2: path to second image to compare
    kwargs: ["maskdir"] contains the directory holding the masks

  Returns:
    None if the images are identical
    A tuple of (errorstring, image) if they're not
  """

  maskdir = None
  if "maskdir" in kwargs:
    maskdir = kwargs["maskdir"]

  im1 = Image.open(file1)
  im2 = Image.open(file2)

  if im1.size != im2.size:
    return ("The images are of different size (%r vs %r)" %
            (im1.size, im2.size), im1)

  diff = ImageChops.difference(im1, im2)

  if maskdir:
    maskfile = os.path.join(maskdir, os.path.basename(file1))
    if os.path.exists(maskfile):
      mask = Image.open(maskfile)

      if mask.size != im1.size:
        return ("The mask is of a different size than the images (%r vs %r)" %
                (mask.size, im1.size), mask)

      diff = ImageChops.multiply(diff, mask.convert(diff.mode))

  if max(diff.getextrema()) != (0, 0):
    return ("The images differ", diff)
  else:
    return None
