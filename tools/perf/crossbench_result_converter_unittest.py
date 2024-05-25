# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import pathlib
import tempfile
import unittest
import unittest.mock

import crossbench_result_converter


class CrossbenchResultConverterTest(unittest.TestCase):

  def call_converter(self, subdir):
    in_dir = (pathlib.Path(__file__).parent / 'testdata/crossbench_output' /
              subdir)

    # Patched version of pathlib.Path.open method, to handle paths that start
    # with /example/path
    def patched_open(p, *args):
      EXAMPLE_PATH = '/example/path'
      try:
        p = in_dir / p.relative_to(EXAMPLE_PATH)
      except ValueError:
        pass
      return open(p, *args)

    with tempfile.TemporaryDirectory() as out_dir:
      histogram_path = pathlib.Path(out_dir) / 'perf_results.json'
      with unittest.mock.patch.object(pathlib.Path, 'open', patched_open):
        crossbench_result_converter.convert(in_dir, histogram_path)
      with histogram_path.open() as f:
        return json.load(f)

  def list_to_dict(self, histogram):
    """Convert histogram data from list to dict for easier checking."""

    result = {}
    for item in histogram:
      if 'name' in item:
        result[item['name']] = item
    return result

  def process_crossbench_result(self, subdir):
    return self.list_to_dict(self.call_converter(subdir))

  def check_result(self, result, metric, value, unit):
    sample_values = result[metric]['sampleValues']
    self.assertIsInstance(sample_values, list)
    self.assertEqual(len(sample_values), 1)
    self.assertAlmostEqual(sample_values[0], value)
    self.assertEqual(result[metric]['unit'], unit)

  def test_jetstream_2_1(self):
    result = self.process_crossbench_result('jetstream_2.1')
    self.check_result(result, 'Total', 188.41971083937912,
                      'unitless_biggerIsBetter')
    self.check_result(result, '3d-cube-SP', 396.15488423869766,
                      'unitless_biggerIsBetter')

  def test_motionmark_1_2(self):
    result = self.process_crossbench_result('motionmark_1.2')
    self.check_result(result, 'score', 896.5337146572772,
                      'unitless_biggerIsBetter')
    self.check_result(result, 'Suits', 884.8798336963456,
                      'unitless_biggerIsBetter')

  def test_motionmark_1_3(self):
    result = self.process_crossbench_result('motionmark_1.3')
    self.check_result(result, 'score', 455.7206962615359,
                      'unitless_biggerIsBetter')
    self.check_result(result, 'Suits', 443.96956820438845,
                      'unitless_biggerIsBetter')

  def test_speedometer_2_1(self):
    result = self.process_crossbench_result('speedometer_2.1')
    self.check_result(result, 'Score', 245.31113999440205,
                      'unitless_biggerIsBetter')
    self.check_result(result, 'jQuery-TodoMVC', 228.69504639975204,
                      'ms_smallerIsBetter')

  def test_speedometer_3_0(self):
    result = self.process_crossbench_result('speedometer_3.0')
    self.check_result(result, 'Score', 13.271380101090333,
                      'unitless_biggerIsBetter')
    self.check_result(result, 'TodoMVC-jQuery', 259.5599999189377,
                      'ms_smallerIsBetter')


if __name__ == '__main__':
  unittest.main()
