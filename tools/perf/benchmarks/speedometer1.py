# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Apple's Speedometer performance benchmark.

Speedometer measures simulated user interactions in web applications.

The current benchmark uses TodoMVC to simulate user actions for adding,
completing, and removing to-do items. Speedometer repeats the same actions using
DOM APIs - a core set of web platform APIs used extensively in web applications-
as well as six popular JavaScript frameworks: Ember.js, Backbone.js, jQuery,
AngularJS, React, and Flight. Many of these frameworks are used on the most
popular websites in the world, such as Facebook and Twitter. The performance of
these types of operations depends on the speed of the DOM APIs, the JavaScript
engine, CSS style resolution, layout, and other technologies.
"""
from telemetry import benchmark

import page_sets
from benchmarks import press


@benchmark.Info(emails=['cbruni@chromium.org', 'vahl@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://browserbench.org/Speedometer')
class Speedometer10(press._PressBenchmark):  # pylint: disable=protected-access
  """Speedometer1.0 benchmark.
  Explicitly named version."""
  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_speedometer1.0'

  def CreateStorySet(self, options):
    return page_sets.Speedometer1StorySet()


@benchmark.Info(emails=['cbruni@chromium.org', 'vahl@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://browserbench.org/Speedometer')
class Speedometer(Speedometer10):
  """Speedometer 1 benchmark."""
  @classmethod
  def Name(cls):
    return 'speedometer'


@benchmark.Info(emails=['cbruni@chromium.org', 'vahl@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://browserbench.org/Speedometer')
class V8SpeedometerFuture(Speedometer):
  """Speedometer 1 benchmark with the V8 flag --future.

  Shows the performance of upcoming V8 VM features.
  """
  @classmethod
  def Name(cls):
    return 'speedometer-future'

  def SetExtraBrowserOptions(self, options):
    super(V8SpeedometerFuture, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-features=V8VmFuture')
