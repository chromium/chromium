# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
from selenium import webdriver

import json
import logging
import selenium
import subprocess
import sys
import time
import traceback

DEFAULT_STP_DRIVER_PATH = '/Applications/Safari Technology Preview.app/Contents/MacOS/safaridriver'

# Maximum number of times the benchmark will be run before giving up.
MAX_ATTEMPTS = 6


class BrowserBench(object):
  def __init__(self, name, version):
    # Log more information to help identify failures.
    logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)
    self._name = name
    self._version = version
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
  def _CreateSafariDriver(optargs):
    params = {}
    if optargs.executable:
      params['exexutable_path'] = optargs.executable
    if optargs.browser == 'stp':
      safari_options = webdriver.safari.options.Options()
      safari_options.use_technology_preview = 1
      params['desired_capabilities'] = {
          'browserName': safari_options.capabilities['browserName']
      }
      # Stp requires executable_path. If the path is not supplied use the
      # typical location.
      if not optargs.executable:
        params['executable_path'] = DEFAULT_STP_DRIVER_PATH
    return webdriver.Safari(**params)

  @staticmethod
  def _CreateDriver(optargs):
    if optargs.browser == 'chrome':
      return BrowserBench._CreateChromeDriver(optargs)
    elif optargs.browser == 'safari' or optargs.browser == 'stp':
      for i in range(0, 10):
        try:
          return BrowserBench._CreateSafariDriver(optargs)
        except selenium.common.exceptions.SessionNotCreatedException as e:
          traceback.print_exc(e)
          logging.info('Connecting to Safari failed, will try again')
          time.sleep(5)
      logging.warning('Failed to connect to Safari, this likely means Safari '
                      'is running something else')
      return None
    else:
      return None

  @staticmethod
  def _KillBrowser(optargs):
    if optargs.browser == 'safari' or optargs.browser == 'stp':
      browser_process_name = ('Safari' if optargs.browser == 'safari' else
                          'Safari Technology Preview')
      logging.warning('Killing Safari')
      subprocess.run(['killall', '-9', browser_process_name])
      # Sleep for a little bit to ensure the kill happened.
      time.sleep(5)

      # safaridriver may be wedged, kill it too.
      logging.warning('Killing safaridriver')
      subprocess.run(['killall', '-9', 'safaridriver'])
      # Sleep for a little bit to ensure the kill happened.
      time.sleep(5)

      logging.warning('Continuing after kill')
      return
    # This logic is primarily for Safari, which seems to occasionally hang. Will
    # implement for Chrome if necessary.
    logging.warning('Not handling kill of chrome, if this is hit and test '
                    'fails, implement it')

  def _CreateDriverAndRun(self, optargs):
    logging.info('Creating Driver')
    driver = BrowserBench._CreateDriver(optargs)
    if not driver:
      raise Exception('failed to create driver')
    driver.set_window_size(900, 780)
    logging.info('About to run test')
    return self.RunAndExtractMeasurements(driver, optargs)

  def _ConvertMeasurementsToSkiaFormat(self, measurements):
    '''
    Processes the results from RunAndExtractMeasurements() into the format used
    by skia, which is:
    An array of dictionaries. Each dictionary contains a single result.
    Expected values in the dictionary are:
      'key': a dictionary that contains the following entries:
        'sub-test': the sub test. For the final score, this is not present.
        'value': the type of measurement: 'score', 'max'...
      'measurement': the measured value.
    '''
    all_results = []
    for suite, results in measurements.items():
      for result in results if isinstance(results, list) else [results]:
        converted_result = {
            'key': {
                'value': result['value']
            },
            'measurement': result['measurement']
        }
        if suite != 'score':
          converted_result['key']['sub-test'] = suite
          converted_result['key']['type'] = 'sub-test'
        else:
          converted_result['key']['type'] = 'rollup'
        all_results.append(converted_result)
    return all_results

  def _ProduceOutput(self, measurements, extra_key_values):
    '''
    extra_key_values is a dictionary of arbitrary key/value pairs added to the
    results.
    '''
    data = {
        'version': 1,
        'git_hash': self._githash,
        'key': {
            'test': self._name,
            'version': self._version,
            'browser': self._browser,
        },
        'results': self._ConvertMeasurementsToSkiaFormat(measurements)
    }
    data['key'].update(extra_key_values)
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

    logging.info('Script starting')

    parser = OptionParser()
    parser.add_option('-b',
                      '--browser',
                      dest='browser',
                      help="""The browser to use. One of chrome, safari, or stp
                              (Safari Technology Preview).""")
    parser.add_option('-e',
                      '--executable-path',
                      dest='executable',
                      help="""Path to the executable to the driver binary. For
                              safari this is the path to safaridriver.""")
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
    parser.add_option('--extra-keys',
                      dest='extra_key_value_pairs',
                      help='Comma separated key/value pairs added to output.')
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

    extra_key_values = {}
    if optargs.extra_key_value_pairs:
      pairs = optargs.extra_key_value_pairs.split(',')
      assert len(pairs) % 2 == 0
      for i in range(0, len(pairs), 2):
        extra_key_values[pairs[i]] = pairs[i + 1]

    self.UpdateParseArgs(optargs)

    run_count = 0
    measurements = False
    # Try running the benchmark a number of times. For whatever reason either
    # Safari or safaridriver does not always complete (based on exceptions it
    # seems the http connection to safari is prematurely closing).
    while not measurements and run_count < MAX_ATTEMPTS:
      run_count += 1
      try:
        measurements = self._CreateDriverAndRun(optargs)
        break
      except Exception as e:
        if run_count < MAX_ATTEMPTS:
          logging.warning('Got exception running, will try again',
                          exc_info=True)
        else:
          logging.critical('Got exception running, retried too many times, '
                           'giving up')
          raise e
      # When rerunning, first try killing the browser in hopes of state
      # resetting.
      BrowserBench._KillBrowser(optargs)

    logging.info('Test completed')
    self._ProduceOutput(measurements, extra_key_values)

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
