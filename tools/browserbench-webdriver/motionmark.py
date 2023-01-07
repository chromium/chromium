# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import browserbench
import time

from selenium.webdriver.support.ui import WebDriverWait


class MotionMark(browserbench.BrowserBench):
  def __init__(self):
    super(MotionMark, self).__init__('motionmark', '1.2')

  def AddExtraParserOptions(self, parser):
    parser.add_option('-s',
                      '--suite',
                      dest='suite',
                      help='Run only the specified suite of tests.')

  def UpdateParseArgs(self, optargs):
    optargs.suite = optargs.suite or 'MotionMark'

  def RunAndExtractMeasurements(self, driver, optargs):
    suite = optargs.suite
    URL = 'https://browserbench.org/MotionMark1.2/'
    driver.get(URL + 'developer.html')
    WebDriverWait(driver, timeout=0).until(lambda driver: driver.execute_script(
        '''return document.querySelector("tree > li") !== undefined'''))
    counter = driver.execute_script('''function Select(benchmark) {
      const list = document.querySelectorAll('.tree > li');
      let counter = 0;
      for (const row of list) {
        const name = row.querySelector('label.tree-label').textContent;
        const checked = name.trim() === benchmark;
        const labels = row.querySelectorAll('input[type=checkbox]');
        for (const label of labels) {
          label.checked = checked;
          if (checked) { ++counter; }
        }
      }
      return counter - 2;  // Each suite has two extra checkboxes. *shrug*
    } return Select("%s");''' % (suite))
    time.sleep(2)
    if counter <= 0:
      return {
          'error': 'No tests found to run for %s' % suite,
      }
    driver.execute_script('window.benchmarkController.startBenchmark()')
    print('Running %d tests.' % counter)
    time.sleep(40 * counter)  # Each test takes approximately 40 seconds.
    while True:
      results = driver.execute_script(
          '''return window.benchmarkRunnerClient.results._results ?
                  window.benchmarkRunnerClient.results.results[0] :
                  undefined''')
      if results: break
      print('Test still running? Trying again in a few seconds.')
      time.sleep(10)

    def _extractScore(results):
      return [{
          'value': 'score',
          'measurement': results['score']
      }, {
          'value': 'min',
          'measurement': results['scoreLowerBound'],
      }, {
          'value': 'max',
          'measurement': results['scoreUpperBound'],
      }]

    measurements = {'score': _extractScore(results)}
    for suite in results['testsResults']:
      for test in results['testsResults'][suite]:
        s = results['testsResults'][suite][test]
        measurements[test] = _extractScore(s)
    return measurements


def main():
  MotionMark().Run()


if __name__ == '__main__':
  main()
