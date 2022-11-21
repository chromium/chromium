# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import browserbench
import time
import logging

from selenium.webdriver.support.ui import WebDriverWait


class Speedometer(browserbench.BrowserBench):
  def __init__(self):
    super(Speedometer, self).__init__('speedometer', '2.1')

  def AddExtraParserOptions(self, parser):
    pass

  def UpdateParseArgs(self, optargs):
    pass

  def RunAndExtractMeasurements(self, driver, optargs):
    URL = 'https://browserbench.org/Speedometer2.1/'
    driver.get(URL)
    # Wait a short amount of time for the system to settle down before starting.
    time.sleep(2)
    WebDriverWait(driver, timeout=60).until(
        lambda driver: driver.execute_script('''return Suites !== undefined &&
                         window.benchmarkClient !== undefined'''))
    logging.info('Page should be ready, test count=%s' %
                 driver.execute_script('return Suites.length;'))
    driver.execute_script('startTest();')
    finished = False
    # This gives 3 minutes for the test to run before stopping. Generally the
    # test takes less than a minute, so this should be plenty of time.
    for i in range(3):
      time.sleep(60)
      logging.info('Checking if done')
      stepCount = driver.execute_script('''
          return benchmarkClient.stepCount !== undefined ?
          benchmarkClient.stepCount : -1''')
      finishedTestCount = driver.execute_script('''
          return benchmarkClient.stepCount !== undefined ?
          benchmarkClient._finishedTestCount : -1''')
      logging.info('Checking if done, stepCount %s finishedTestCount %s' %
                   (stepCount, finishedTestCount))
      if stepCount != -1 and stepCount == finishedTestCount:
        finished = True
        break
    if not finished:
      logging.info('Test did not complete in time, restarting')
      raise RuntimeError('Test did not complete in time')

    logging.info('Test done, extracting measurements')
    results = driver.execute_script('''
        return benchmarkClient._computeResults(
            benchmarkClient._measuredValuesList,
            benchmarkClient.displayUnit);''')
    measurements = {
        'score': {
            'value': 'score',
            'measurement': results['mean'],
        }
    }
    return measurements


def main():
  Speedometer().Run()


if __name__ == '__main__':
  main()
