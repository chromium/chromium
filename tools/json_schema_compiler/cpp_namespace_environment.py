# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class CppNamespaceEnvironment(object):

  def __init__(self, namespace_pattern):
    self.namespace_pattern = namespace_pattern
