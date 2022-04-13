# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
from selenium import webdriver

import json
import sys
import time


class BrowserBench(object):
  def __init__(self, name):
    self._name = name
    self._output = None
    self._githash = None
    self._browser = None

  @staticmethod
  def _CreateChromeDriver(optargs):
    options = webdriver.ChromeOptions()
    options.add_argument('enable-benchmarking')
    if optargs.arguments:
      for arg in optargs.arguments.split(','):
        options.add_argument(arg)
    if optargs.chrome_path:
      options.binary_location = optargs.chrome_path
    service = webdriver.chrome.service.Service(
        executable_path=optargs.executable)
    chrome = webdriver.Chrome(service=service, options=options)
    return chrome

  @staticmethod
  def _CreateDriver(optargs):
    if optargs.browser == 'chrome':
      return BrowserBench._CreateChromeDriver(optargs)
    elif optargs.browser == 'safari':
      return webdriver.Safari(executable_path=optargs.executable
                              ) if optargs.executable else webdriver.Safari()
    else:
      return None

  def _ProduceOutput(self, measurements):
    data = {
        'version': 1,
        'git_hash': self._githash,
        'key': {
            'test': self._name,
            'browser': self._browser,
        },
        'measurements': measurements
    }
    print(json.dumps(data, sort_keys=True, indent=2, separators=(',', ': ')))
    if self._output:
      with open(self._output, 'w') as file:
        file.write(json.dumps(data))

  def Run(self):
    '''Runs the benchmark.

    Runs the benchmark end-to-end, starting from parsing the command line
    arguments (see README.md for details), and ending with producing the output
    to the standard output, as well as any output file specified in the command
    line arguments.
    '''

    parser = OptionParser()
    parser.add_option('-b',
                      '--browser',
                      dest='browser',
                      help='The browser to use to run MotionMark in.')
    parser.add_option('-e',
                      '--executable-path',
                      dest='executable',
                      help='Path to the executable to the driver binary.')
    parser.add_option('-a',
                      '--arguments',
                      dest='arguments',
                      help='Extra arguments to pass to the browser.')
    parser.add_option('-g',
                      '--githash',
                      dest='githash',
                      help='A git-hash associated with this run.')
    parser.add_option('-o',
                      '--output',
                      dest='output',
                      help='Path to the output json file.')
    parser.add_option(
        '--chrome-path',
        dest='chrome_path',
        help=
        'Path of the chrome executable. If not specified, the default is picked'
        ' up from chromedriver.')
    self.AddExtraParserOptions(parser)

    (optargs, args) = parser.parse_args()
    self._githash = optargs.githash or 'deadbeef'
    self._output = optargs.output
    self._browser = optargs.browser

    self.UpdateParseArgs(optargs)

    driver = BrowserBench._CreateDriver(optargs)
    if not driver:
      sys.stderr.write('Could not create a driver. Aborting.\n')
      sys.exit(1)
    driver.set_window_size(900, 780)

    measurements = self.RunAndExtractMeasurements(driver, optargs)
    self._ProduceOutput(measurements)

  def AddExtraParserOptions(self, parser):
    pass

  def UpdateParseArgs(self, optargs):
    pass

  def RunAndExtractMeasurements(self, driver, optargs):
    '''Runs the benchmark and returns the result.

    The result is a dictionary with an entry per suite as well as an entry for
    the overall score. The value of each entry is a list of dictionaries, with
    the key 'value' denoting the type of value. For example:
    {
      'score': [{ 'value': 'score',
                  'measurement': 10 }],
      'Suite1': [{ 'value': 'score',
                   'measurement': 11 }],
    }
    The has an overall score of 10, and the suite 'Suite1' has an overall
    score of 11. Additional values types are 'min' and 'max', these are
    optional as not all tests provide them.
    '''
    return {'error': 'Benchmark has not been set up correctly.'}
