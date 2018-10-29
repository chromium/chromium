# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Mozilla's Kraken JavaScript benchmark."""

import json
import os

from core import perf_benchmark

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.page import legacy_page_test
from telemetry import story
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar


DESCRIPTIONS = {
    'ai-astar':
        'This benchmark uses the [A* search algorithm]'
        '(http://en.wikipedia.org/wiki/A*_search_algorithm) to automatically '
        'plot an efficient path between two points, in the presence of '
        'obstacles. Adapted from code by [Brian Gringstead]'
        '(http://www.briangrinstead.com/blog/astar-search-algorithm-in-'
        'javascript).',
    'audio-beat-detection':
        'This benchmark performs [beat detection]'
        '(http://en.wikipedia.org/wiki/Beat_detection) on an Audio sample '
        'using [code](http://beatdetektor.svn.sourceforge.net/viewvc'
        '/beatdetektor/trunk/core/js/beatdetektor.js?revision=18&view=markup) '
        'from [BeatDetektor](http://www.cubicproductions.com/index.php'
        '?option=com_content&view=article&id=67&Itemid=82) and '
        '[DSP.js](http://github.com/corbanbrook/dsp.js/).',
    'audio-dft':
        'This benchmark performs a [Discrete Fourier Transform]'
        '(http://en.wikipedia.org/wiki/Discrete_Fourier_transform) on an '
        'Audio sample using code from [DSP.js]'
        '(http://github.com/corbanbrook/dsp.js).',
    'audio-fft':
        'This benchmark performs a [Fast Fourier Transform]'
        '(http://en.wikipedia.org/wiki/Fast_Fourier_transform) on an Audio '
        'sample using code from [DSP.js]'
        '(http://github.com/corbanbrook/dsp.js/).',
    'audio-oscillator':
        'This benchmark generates a soundwave using code from [DSP.js]'
        '(http://github.com/corbanbrook/dsp.js/).',
    'imaging-darkroom':
        'This benchmark performs a variety of photo manipulations such as '
        'Fill, Brightness, Contrast, Saturation, and Temperature.',
    'imaging-desaturate':
        'This benchmark [desaturates]'
        '(http://en.wikipedia.org/wiki/Colorfulness) a photo using code from '
        '[Pixastic](http://www.pixastic.com/).',
    'imaging-gaussian-blur':
        'This benchmark performs a [Gaussian blur]'
        '(http://en.wikipedia.org/wiki/Gaussian_blur) on a photo.',
    'json-parse-financial':
        'This benchmark parses [JSON](http://www.json.org) records.',
    'json-stringify-tinderbox':
        'This benchmark serializes [Tinderbox]'
        '(http://tests.themasta.com/tinderboxpushlog/?tree=Firefox) build '
        'data to [JSON](http://www.json.org).',
}


def _Mean(l):
  return float(sum(l)) / len(l) if len(l) > 0 else 0.0


class _KrakenMeasurement(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(_KrakenMeasurement, self).__init__()


  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptCondition(
        'document.title.indexOf("Results") != -1', timeout=700)
    tab.WaitForDocumentReadyStateToBeComplete()

    result_dict = json.loads(tab.EvaluateJavaScript("""
        var formElement = document.getElementsByTagName("input")[0];
        decodeURIComponent(formElement.value.split("?")[1]);
        """))
    total = 0
    for key in result_dict:
      if key == 'v':
        continue
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, key, 'ms', result_dict[key], important=False,
          description=DESCRIPTIONS.get(key)))
      total += _Mean(result_dict[key])

    # TODO(tonyg/nednguyen): This measurement shouldn't calculate Total. The
    # results system should do that for us.
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Total', 'ms', total,
        description='Total of the means of the results for each type '
                    'of benchmark in [Mozilla\'s Kraken JavaScript benchmark]'
                    '(http://krakenbenchmark.mozilla.org/)'))


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink>JavaScript')
class Kraken(perf_benchmark.PerfBenchmark):
  """Mozilla's Kraken JavaScript benchmark.

  http://krakenbenchmark.mozilla.org/
  """
  test = _KrakenMeasurement

  @classmethod
  def Name(cls):
    return 'kraken'

  def CreateStorySet(self, options):
    ps = story.StorySet(
        archive_data_file='../page_sets/data/kraken.json',
        base_dir=os.path.dirname(os.path.abspath(__file__)),
        cloud_storage_bucket=story.PARTNER_BUCKET)
    ps.AddStory(page_module.Page(
        'http://krakenbenchmark.mozilla.org/kraken-1.1/driver.html',
        ps, ps.base_dir,
        name='http://krakenbenchmark.mozilla.org/kraken-1.1/driver.html'))
    return ps
