# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
from selenium import webdriver
from selenium.webdriver.support.ui import WebDriverWait

import browserbench
import json
import time

URL = 'https://browserbench.org/JetStream/'


# JetStream uses the element 'status' to contain status information.
def _DoesElementHaveClass(driver, element_name, class_value):
  return driver.execute_script(
      'return document.getElementById("%s").classList.contains("%s")' %
      (element_name, class_value))


def _IsLoading(driver):
  return _DoesElementHaveClass(driver, 'status', 'loading')


def _IsDone(driver):
  return _DoesElementHaveClass(driver, 'result-summary', 'done')


def _GetResults(driver, optargs):
  # All benchmarks include a score, but not necessarily a max (and no min). If
  # the max is provided, it is available via subTimes().
  result = json.loads(
      driver.execute_script('''let results = {};
       let allScores = [];
       for (let benchmark of JetStream.benchmarks) {
         allScores.push(benchmark.score);
         results[benchmark.name] = [{
           'value': 'score',
           'measurement': benchmark.score,
         }];
         if (benchmark.subTimes()['Worst']) {
           results[benchmark.name].push({
             'value': 'max',
             'measurement': benchmark.subTimes()['Worst'],
           });
         }
       }
       results['score'] = [{
         'value': 'score',
         'measurement': geomean(allScores),
       }];
       return JSON.stringify(results);'''))
  return result


class JetStream(browserbench.BrowserBench):
  def __init__(self):
    super(JetStream, self).__init__('JetStream', '2')

  def AddExtraParserOptions(self, parser):
    parser.add_option(
        '-s',
        '--suite',
        dest='suite',
        help='Run only the specified suite of tests (comma separated).')

  def RunAndExtractMeasurements(self, driver, optargs):
    driver.get(URL)

    # Jetstream removes the status 'loading' once ready.
    time.sleep(10)
    WebDriverWait(driver,
                  timeout=300).until(lambda driver: not _IsLoading(driver))

    print('no longer loading, about to start')
    # JetStream waits another 4 seconds before starting (add one as 'loading'
    # is removed a bit earlier).
    time.sleep(5)

    if optargs.suite:
      print('running suites', optargs.suite)
      driver.execute_script('JetStream.benchmarks = [];')
      for suite in optargs.suite.split(','):
        driver.execute_script('addTestsByGroup(' + suite + ')')
    else:
      print('running all')

    driver.execute_script('JetStream.start()')

    WebDriverWait(driver, timeout=600).until(lambda driver: _IsDone(driver))
    return _GetResults(driver, optargs)


def main():
  JetStream().Run()


if __name__ == '__main__':
  main()
