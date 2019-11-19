# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json

from page_sets import press_story
from telemetry import story


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


class KrakenStory(press_story.PressStory):
  URL='http://krakenbenchmark.mozilla.org/kraken-1.1/driver.html'

  def ExecuteTest(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        'document.title.indexOf("Results") != -1', timeout=700)
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

  def ParseTestResults(self, action_runner):
    result_dict = json.loads(action_runner.EvaluateJavaScript("""
        var formElement = document.getElementsByTagName("input")[0];
        decodeURIComponent(formElement.value.split("?")[1]);
        """))
    total = 0
    for key in result_dict:
      if key == 'v':
        continue
      self.AddMeasurement(key, 'ms', result_dict[key],
                          description=DESCRIPTIONS.get(key))
      total += _Mean(result_dict[key])

    # TODO(tonyg/nednguyen): This measurement shouldn't calculate Total. The
    # results system should do that for us.
    self.AddMeasurement(
        'Total', 'ms', total,
        description='Sum of the mean runtime for each type of benchmark in '
                    "Mozilla's Kraken JavaScript benchmark")


class KrakenStorySet(story.StorySet):
  def __init__(self):
    super(KrakenStorySet, self).__init__(
        archive_data_file='data/kraken.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    self.AddStory(KrakenStory(self))
