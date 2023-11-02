# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Output formatter for HistogramSet Results Format.

Format specification:
https://github.com/catapult-project/catapult/blob/master/docs/histogram-set-json-format.md
"""

import json
import logging
import os


# Output file in HistogramSet format.
OUTPUT_FILENAME = 'histograms.json'


def ProcessHistogramDicts(histogram_dicts, options):
  """Write histograms in output_dir."""
  output_file = os.path.join(options.output_dir, OUTPUT_FILENAME)
  if not options.reset_results and os.path.isfile(output_file):
    with open(output_file) as input_stream:
      try:
        histogram_dicts += json.load(input_stream)
      except ValueError:
        logging.warning(
            'Found existing histograms json but failed to parse it.')

  with open(output_file, 'w') as output_stream:
    json.dump(histogram_dicts, output_stream)

  return output_file
