# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.results_processor.formatters import csv_output
from core.results_processor.formatters import histograms_output
from core.results_processor.formatters import html_output
from core.results_processor.formatters import json3_output


FORMATTERS = {
    'json-test-results': json3_output,
    'histograms': histograms_output,
    'html': html_output,
    'csv': csv_output,
}
