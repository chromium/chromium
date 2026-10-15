# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock

import setup_modules  # pylint: disable=unused-import


import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.print_histogram_names as print_histogram_names


_HISTOGRAM_XML = """\
<histogram-configuration>
  <histograms>
    <histogram name="{name}" units="count" expires_after="never">
      <owner>test@chromium.org</owner>
      <summary>Records a test value.</summary>
    </histogram>
  </histograms>
</histogram-configuration>
"""


class PrintHistogramNamesTest(unittest.TestCase):
  def setUp(self):
    temp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(temp_dir.cleanup)
    paths = []
    for name in ['Test.Z', 'Test.A']:
      path = pathlib.Path(temp_dir.name) / f'{name}.xml'
      path.write_text(_HISTOGRAM_XML.format(name=name), encoding='utf-8')
      paths.append(str(path))
    paths_patch = mock.patch.object(histogram_paths, 'ALL_XMLS', paths)
    paths_patch.start()
    self.addCleanup(paths_patch.stop)
    self.opened_files = []
    self.real_open = open

  def _OpenOneAtATime(self, *args, **kwargs):
    self.assertTrue(all(f.closed for f in self.opened_files))
    f = self.real_open(*args, **kwargs)
    self.opened_files.append(f)
    self.addCleanup(f.close)
    return f

  def _RunMain(self, *args):
    output = io.StringIO()
    with mock.patch('builtins.open', side_effect=self._OpenOneAtATime):
      with contextlib.redirect_stdout(output):
        print_histogram_names.main(['print_histogram_names.py', *args])
    self.assertEqual(len(self.opened_files), 2)
    self.assertTrue(all(f.closed for f in self.opened_files))
    return output.getvalue()

  def testPrintsSortedNamesWithOneOpenFile(self):
    self.assertEqual(self._RunMain(), 'Test.A\nTest.Z\n')

  def testDiffReadsHistoricalStreamsAndSkipsMissingPaths(self):
    previous_xml = _HISTOGRAM_XML.format(name='Test.Removed').encode('utf-8')
    with mock.patch.object(
      histogram_paths, 'ALL_XMLS_RELATIVE', ['first.xml', 'second.xml']
    ):
      with mock.patch.object(
        print_histogram_names.subprocess,
        'check_output',
        side_effect=[previous_xml, subprocess.CalledProcessError(1, 'git')],
      ) as check_output:
        self.assertEqual(
          self._RunMain('--diff', 'base'),
          '2 histograms added:\nTest.A\nTest.Z\n'
          '1 histograms removed:\nTest.Removed\n',
        )
    self.assertEqual(
      check_output.call_args_list,
      [
        mock.call(('git', 'cat-file', '--textconv', 'base:first.xml')),
        mock.call(('git', 'cat-file', '--textconv', 'base:second.xml')),
      ],
    )


if __name__ == '__main__':
  unittest.main()
