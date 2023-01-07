# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import legacy_page_test
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class RepaintDesktopPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.REPAINT_DESKTOP]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(RepaintDesktopPage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.readyState == "complete"', timeout=30)
    action_runner.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

    mode = 'viewport'
    width = None
    height = None
    args = {}
    args['mode'] = mode
    if width:
      args['width'] = width
    if height:
      args['height'] = height

    # Enqueue benchmark
    action_runner.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.id =
            chrome.gpuBenchmarking.runMicroBenchmark(
                "invalidation_benchmark",
                function(value) {},
                {{ args }}
            );
        """,
        args=args)

    micro_benchmark_id = action_runner.EvaluateJavaScript(
      'window.benchmark_results.id')
    if not micro_benchmark_id:
      raise legacy_page_test.MeasurementFailure(
          'Failed to schedule invalidation_benchmark.')

    with action_runner.CreateInteraction('Repaint'):
      action_runner.RepaintContinuously(seconds=5)

    action_runner.ExecuteJavaScript("""
        window.benchmark_results.message_handled =
            chrome.gpuBenchmarking.sendMessageToMicroBenchmark(
                  {{ micro_benchmark_id }}, {
                    "notify_done": true
                  });
        """,
        micro_benchmark_id=micro_benchmark_id)


class RepaintAmazon2018Page(RepaintDesktopPage):
  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """
  BASE_NAME = 'repaint_amazon'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_amazon.html'


class RepaintCNN2018Page(RepaintDesktopPage):
  """# Why: #2 news worldwide"""
  BASE_NAME = 'repaint_cnn'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_cnn.html'


class RepaintFacebook2018Page(RepaintDesktopPage):
  """Why: #3 (Alexa global)"""
  BASE_NAME = 'repaint_facebook'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_facebook.html'


class RepaintGoogleSearch2018Page(RepaintDesktopPage):
  """Why: Top Google property; a Google tab is often open"""
  BASE_NAME = 'repaint_google_search'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_google_search.html'


class RepaintInstagram2018Page(RepaintDesktopPage):
  """Why: A top social site"""
  BASE_NAME = 'repaint_instagram'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_instagram.html'


class RepaintReddit2018Page(RepaintDesktopPage):
  """Why: #1 news worldwide"""
  BASE_NAME = 'repaint_reddit'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_reddit.html'


class RepaintTheVerge2018Page(RepaintDesktopPage):
  """Why: Top tech blog"""
  BASE_NAME = 'repaint_theverge'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_theverge.html'


class RepaintTwitter2018Page(RepaintDesktopPage):
  """Why: A top social site"""
  BASE_NAME = 'repaint_twitter'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_twitter.html'


class RepaintWikipedia2018Page(RepaintDesktopPage):
  """Why: #5 (Alexa global)"""
  BASE_NAME = 'repaint_wikipedia'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_wikipedia.html'


class RepaintYahoo2018Page(RepaintDesktopPage):
  """Why: #9 (Alexa global)"""
  BASE_NAME = 'repaint_yahoo_homepage'
  YEAR = '2018'
  URL = 'http://vmiura.github.io/snapit-pages/snapit_yahoo_homepage.html'
