# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import story

from page_sets import press_story


class WasmPsPdfKitStory(press_story.PressStory):
  URL = 'https://pspdfkit.com/webassembly-benchmark/'

  def ExecuteTest(self, action_runner):
    action_runner.WaitForElement(text='All done!', timeout_in_seconds=480)

  def ParseTestResults(self, action_runner):
    self.AddJavaScriptMeasurement(
        'Total', 'ms', """
        parseInt(document.querySelector(
          '#root > div > div:nth-child(10) > div.Result >' +
          'div.Result-score > div > div.Score-value'
        ).textContent)
        """)
    self.AddJavaScriptMeasurement(
        'Rendering', 'ms', """
        let text =
        document.querySelector(
        '#Test-Rendering > details > summary > div.Bench-heading > span'
        ).textContent;
        let value = text.substring(0, text.length - 3);
        parseInt(value)
        """)

    self.AddJavaScriptMeasurement(
        'Searching', 'ms', """
        text =
        document.querySelector(
        '#Test-Searching > details > summary > div.Bench-heading > span'
        ).textContent;
        value = text.substring(0, text.length - 3);
        parseInt(value)
        """)

    self.AddJavaScriptMeasurement(
        'Exporting', 'ms', """
        text =
        document.querySelector(
        '#Test-Exporting > details > summary > div.Bench-heading > span'
        ).textContent;
        value = text.substring(0, text.length - 3);
        parseInt(value)
        """)

    self.AddJavaScriptMeasurement(
        'Annotations', 'ms', """
        text =
        document.querySelector(
        '#Test-Annotations > details > summary > div.Bench-heading > span'
        ).textContent;
        value = text.substring(0, text.length - 3);
        parseInt(value)
        """)

    self.AddJavaScriptMeasurement(
        'Initialization', 'ms', """
        text =
        document.querySelector(
        '#Test-Initialization > details > summary > div.Bench-heading > span'
        ).textContent;
        value = text.substring(0, text.length - 3);
        parseInt(value)
        """)


class WasmPsPdfKitStorySet(story.StorySet):
  def __init__(self):
    super(WasmPsPdfKitStorySet,
          self).__init__(archive_data_file='data/WasmPsPdfKit.json',
                         cloud_storage_bucket=story.PUBLIC_BUCKET)

    self.AddStory(WasmPsPdfKitStory(self))
