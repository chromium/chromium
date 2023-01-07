# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from contrib.cluster_telemetry import loading_base_ct

# pylint: disable=protected-access
class LoadingClusterTelemetry(loading_base_ct._LoadingBaseClusterTelemetry):

  @classmethod
  def Name(cls):
    return 'loading.cluster_telemetry'
