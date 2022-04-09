# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
from selenium import webdriver
from selenium.webdriver.support.ui import WebDriverWait

import json
import sys
import time

URL = 'https://browserbench.org/MotionMark1.2/'


def MotionMarkOneWithDriver(driver, driver_name, suite, name):
  url = URL + "developer.html?warmup-length=2000&\
warmup-frame-count=30&\
first-frame-minimum-length=0&\
test-interval=30&\
display=minimal&\
tiles=big&\
controller=ramp&\
frame-rate=50&\
time-measurement=performance&\
suite-name=%s&\
test-name=%s&\
complexity=309" % (suite, name)
  driver.get(url)
  time.sleep(40)
  while True:
    result = driver.execute_script(
        'return window.benchmarkRunnerClient.results.results')
    if result: break
    print('Test still running? Trying again in a few seconds.')
    time.sleep(10)

  print('%s,%s,%s,%f' % (driver_name, suite, name, result['score']))


def MotionMarkAll():
  tests = {
      'MotionMark': [
          'Multiply', 'CanvasArcs', 'Leaves', 'Paths', 'CanvasLines', 'Images',
          'Design', 'Suits'
      ],
      'HTMLsuite': [
          "CSSbouncingcircles", "CSSbouncingclippedrects",
          "CSSbouncinggradientcircles", "CSSbouncingblendcircles",
          "CSSbouncingfiltercircles", "CSSbouncingSVGimages",
          "CSSbouncingtaggedimages", "Focus20", "DOMparticlesSVGmasks",
          "CompositedTransforms"
      ],
      'Canvassuite': [
          "canvasbouncingclippedrects", "canvasbouncinggradientcircles",
          "canvasbouncingSVGimages", "canvasbouncingPNGimages", "Strokeshapes",
          "Fillshapes", "Canvasputgetimagedata"
      ],
      'SVGsuite': [
          "SVGbouncingcircles", "SVGbouncingclippedrects",
          "SVGbouncinggradientcircles", "SVGbouncingSVGimages",
          "SVGbouncingPNGimages"
      ],
      'Leavessuite':
      ["TranslateonlyLeaves", "TranslateScaleLeaves", "TranslateOpacityLeaves"],
      'Multiplysuite': [
          "MultiplyCSSopacityonly", "MultiplyCSSdisplayonly",
          "MultiplyCSSvisibilityonly"
      ],
      'Textsuite': [
          "DesignLatinonly12items", "DesignCJKonly12items",
          "DesignRTLandcomplexscriptsonly12items", "DesignLatinonly6items",
          "DesignCJKonly6items", "DesignRTLandcomplexscriptsonly6items"
      ],
      'Suitssuite': [
          "Suitscliponly", "Suitsshapeonly", "Suitsclipshaperotation",
          "Suitsclipshapegradient", "Suitsstatic"
      ],
      '3DGraphics': [
          "TrianglesWebGL",
          # "TrianglesWebGPU"
      ],
      'Basiccanvaspathsuite': [
          "Canvaslinesegmentsbuttcaps", "Canvaslinesegmentsroundcaps",
          "Canvaslinesegmentssquarecaps", "Canvaslinepathbeveljoin",
          "Canvaslinepathroundjoin", "Canvaslinepathmiterjoin",
          "Canvaslinepathwithdashpattern", "Canvasquadraticsegments",
          "Canvasquadraticpath", "Canvasbeziersegments", "Canvasbezierpath",
          "CanvasarcTosegments", "Canvasarcsegments", "Canvasrects",
          "Canvasellipses", "Canvaslinepathfill", "Canvasquadraticpathfill",
          "Canvasbezierpathfill", "CanvasarcTosegmentsfill",
          "Canvasarcsegmentsfill", "Canvasrectsfill", "Canvasellipsesfill"
      ]
  }
  optargs = ParseArgs()
  driver = CreateDriver(optargs)
  driver.set_window_size(850, 700)
  for suite in tests:
    for test in tests[suite]:
      for i in range(5):
        MotionMarkOneWithDriver(driver, optargs.browser, suite, test)


def CreateChromeDriver(optargs):
  options = webdriver.ChromeOptions()
  options.add_argument('enable-benchmarking')
  if optargs.arguments:
    for arg in optargs.arguments.split(','):
      options.add_argument(arg)
  service = webdriver.chrome.service.Service(executable_path=optargs.executable)
  chrome = webdriver.Chrome(service=service, options=options)
  return chrome


def CreateDriver(optargs):
  if optargs.browser == 'chrome':
    return CreateChromeDriver(optargs)
  elif optargs.browser == 'safari':
    return webdriver.Safari(executable_path=optargs.executable
                            ) if optargs.executable else webdriver.Safari()
  else:
    return None


def RunMotionMarkSuite(driver, suite):
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
        if (checked) ++counter;
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
  return results


def ParseArgs():
  parser = OptionParser()
  parser.add_option('-b',
                    '--browser',
                    dest='browser',
                    help='The browser to use to run MotionMark in.')
  parser.add_option('-s',
                    '--suite',
                    dest='suite',
                    help='Run only the specified suite of tests.')
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

  (optargs, args) = parser.parse_args()
  optargs.suite = optargs.suite or 'MotionMark'
  optargs.githash = optargs.githash or 'deadbeef'
  return optargs


def ProduceOutput(data, output_file):
  print(json.dumps(data, sort_keys=True, indent=2, separators=(',', ': ')))
  if output_file:
    with open(output_file, 'w') as file:
      file.write(json.dumps(data))


def main():
  optargs = ParseArgs()
  driver = CreateDriver(optargs)
  if not driver:
    sys.stderr.write('Could not create a driver. Aborting.\n')
    sys.exit(1)
  driver.set_window_size(900, 780)

  scores = RunMotionMarkSuite(driver, optargs.suite)
  if 'error' in scores:
    ProduceOutput(scores, optargs.output)
    sys.exit(1)

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

  results = {
      'version': 1,
      'git_hash': optargs.githash,
      'key': {
          'test': 'motionmark',
          'browser': optargs.browser,
      },
      'measurements': {
          'score': _extractScore(scores),
      },
  }
  for suite in scores['testsResults']:
    for test in scores['testsResults'][suite]:
      s = scores['testsResults'][suite][test]
      results['measurements'][test] = _extractScore(s)

  ProduceOutput(results, optargs.output)


if __name__ == '__main__':
  main()
