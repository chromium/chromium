# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from telemetry import page
from contrib.vr_benchmarks import shared_vr_page_state as vr_state

WEBVR_SAMPLE_DIR = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..', 'chrome', 'test',
    'data', 'xr', 'webvr_info', 'samples')


WEBXR_SAMPLE_DIR = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..', 'third_party',
        'webxr_test_pages', 'webxr-samples')


class _VrXrSamplePage(page.Page):
  """Superclass for all VR and XR sample pages."""

  def __init__(self, sample_directory, sample_page, page_set,
      url_parameters=None, extra_browser_args=None):
    url = '%s.html' % sample_page
    if url_parameters is not None:
      url += '?' + '&'.join(url_parameters)
    name = url.replace('.html', '')
    # Replace characters that are unsupported by the perf dashboard here so that
    # the name reported on the dashboard can be used as a story filter.
    # We don't use a the \W+ regex like other benchmarks because we need to
    # keep certain non-alphanumeric characters around for backwards naming
    # compatibility. This regex should replace anything except alphanumeric,
    # question mark, dash, and period characters with underscores.
    name = re.sub(r'[^a-zA-Z\d\?\-\.]+', '_', name)
    url = 'file://' + os.path.join(sample_directory, url)
    super(_VrXrSamplePage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=vr_state.SharedVrPageStateFactory)
    self._shared_page_state = None

  def Run(self, shared_state):
    self._shared_page_state = shared_state
    super(_VrXrSamplePage, self).Run(shared_state)

  @property
  def platform(self):
    return self._shared_page_state.platform


class VrSamplePage(_VrXrSamplePage):
  """Superclass for all VR sample pages."""

  def __init__(self, sample_page, page_set, url_parameters=None,
      extra_browser_args=None):
    super(VrSamplePage, self).__init__(
        sample_directory=WEBVR_SAMPLE_DIR,
        sample_page=sample_page,
        page_set=page_set,
        url_parameters=url_parameters,
        extra_browser_args=extra_browser_args)


class XrSamplePage(_VrXrSamplePage):
  """Superclass for all XR sample pages."""

  def __init__(self, sample_page, page_set, url_parameters=None,
      extra_browser_args=None):
    super(XrSamplePage, self).__init__(
        sample_directory=WEBXR_SAMPLE_DIR,
        sample_page=sample_page,
        page_set=page_set,
        url_parameters=url_parameters,
        extra_browser_args=extra_browser_args)

  @property
  def serving_dir(self):
    # The default implementation of serving_dir results in the WebXR pages not
    # loading properly since the JS resources are in webxr_samples/js/, and the
    # default implementation results in webxr_samples/tests/ being the serving
    # directory.
    return WEBXR_SAMPLE_DIR
