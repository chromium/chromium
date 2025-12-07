# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cc_generator import CCGenerator
from h_generator import HGenerator


class CppGenerator(object):

  def __init__(self, type_generator):
    self.h_generator = HGenerator(type_generator)
    self.cc_generator = CCGenerator(type_generator)
