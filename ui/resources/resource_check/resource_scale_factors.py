# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools, and see
https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md
for the rules we're checking against here.
"""


import os
import struct


class InvalidPNGException(Exception):
  pass


class ResourceScaleFactors(object):
  """Verifier of image dimensions for Chromium resources.

  This class verifies the image dimensions of resources in the various
  resource subdirectories.

  Attributes:
      paths: An array of tuples giving the folders to check and their
          relevant scale factors. For example:

          [(100, 'default_100_percent'), (200, 'default_200_percent')]
  """

  def __init__(self, input_api, output_api, paths):
    """ Initializes ResourceScaleFactors with paths."""
    self.input_api = input_api
    self.output_api = output_api
    self.paths = paths

  def RunChecks(self):
    """Verifies the scale factors of resources being added or modified.

    Returns:
        An array of presubmit errors if any images were detected not
        having the correct dimensions.
    """
    def ImageSize(filename):
      with open(filename, 'rb', buffering=0) as f:
        data = f.read(24)
      if data[:8] != b'\x89PNG\r\n\x1A\n' or data[12:16] != b'IHDR':
        raise InvalidPNGException
      return struct.unpack('>ii', data[16:24])

    # Returns a list of valid scaled image sizes. The valid sizes are the
    # floor and ceiling of (base_size * scale_percent / 100). This is equivalent
    # to requiring that the actual scaled size is less than one pixel away from
    # the exact scaled size.
    def ValidSizes(base_size, scale_percent):
      return sorted(set([(base_size * scale_percent) / 100,
                         (base_size * scale_percent + 99) / 100]))

    repository_path = self.input_api.os_path.relpath(
        self.input_api.PresubmitLocalPath(),
        self.input_api.change.RepositoryRoot())
    results = []

    # Check for affected files in any of the paths specified.
    affected_files = self.input_api.AffectedFiles(include_deletes=False)
    files = []
    for f in affected_files:
      for path_spec in self.paths:
        path_root = self.input_api.os_path.join(
            repository_path, path_spec[1])
        if (f.LocalPath().endswith('.png') and
            f.LocalPath().startswith(path_root)):
          # Only save the relative path from the resource directory.
          relative_path = self.input_api.os_path.relpath(f.LocalPath(),
              path_root)
          if relative_path not in files:
            files.append(relative_path)

    corrupt_png_error = ('Corrupt PNG in file %s. Note that binaries are not '
        'correctly uploaded to the code review tool and must be directly '
        'submitted using the dcommit command.')
    for f in files:
      base_image = self.input_api.os_path.join(self.paths[0][1], f)
      if not os.path.exists(base_image):
        results.append(self.output_api.PresubmitError(
            'Base image %s does not exist' % self.input_api.os_path.join(
            repository_path, base_image)))
        continue
      try:
        base_dimensions = ImageSize(base_image)
      except InvalidPNGException:
        results.append(self.output_api.PresubmitError(corrupt_png_error %
            self.input_api.os_path.join(repository_path, base_image)))
        continue
      # Find all scaled versions of the base image and verify their sizes.
      for i in range(1, len(self.paths)):
        image_path = self.input_api.os_path.join(self.paths[i][1], f)
        if not os.path.exists(image_path):
          continue
        # Ensure that each image for a particular scale factor is the
        # correct scale of the base image.
        try:
          scaled_dimensions = ImageSize(image_path)
        except InvalidPNGException:
          results.append(self.output_api.PresubmitError(corrupt_png_error %
              self.input_api.os_path.join(repository_path, image_path)))
          continue
        for dimension_name, base_size, scaled_size in zip(
            ('width', 'height'), base_dimensions, scaled_dimensions):
          valid_sizes = ValidSizes(base_size, self.paths[i][0])
          if scaled_size not in valid_sizes:
            results.append(self.output_api.PresubmitError(
                'Image %s has %s %d, expected to be %s' % (
                self.input_api.os_path.join(repository_path, image_path),
                dimension_name,
                scaled_size,
                ' or '.join(map(str, valid_sizes)))))
    return results
