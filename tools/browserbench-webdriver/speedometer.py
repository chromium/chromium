# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import browserbench
import time

from selenium.webdriver.support.ui import WebDriverWait


class Speedometer(browserbench.BrowserBench):
  def __init__(self):
    super(Speedometer, self).__init__('speedometer', '2.0')

  def AddExtraParserOptions(self, parser):
    pass

  def UpdateParseArgs(self, optargs):
    pass

  def RunAndExtractMeasurements(self, driver, optargs):
    URL = 'https://browserbench.org/Speedometer2.0/'
    driver.get(URL)
    WebDriverWait(driver,
                  timeout=100000).until(lambda driver: driver.execute_script(
                      '''return window.benchmarkClient !== undefined'''))
    driver.execute_script('''startTest();''')
    WebDriverWait(
        driver, timeout=100000,
        poll_frequency=30).until(lambda driver: driver.execute_script('''
            return benchmarkClient.stepCount &&
            benchmarkClient._finishedTestCount === benchmarkClient.stepCount''')
                                 )
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
