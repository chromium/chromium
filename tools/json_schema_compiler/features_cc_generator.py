# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from code_util import Code
import cpp_util


class CCGenerator(object):

  def Generate(self, feature_defs, source_file, namespace):
    return _Generator(feature_defs, source_file, namespace).Generate()


class _Generator(object):
  """A .cc generator for features.
  """

  def __init__(self, feature_defs, source_file, namespace):
    self._feature_defs = feature_defs
    self._source_file = source_file
    self._source_file_filename, _ = os.path.splitext(source_file)
    self._class_name = cpp_util.ClassName(self._source_file_filename)
    self._namespace = namespace

  def Generate(self):
    """Generates a Code object for features.
    """
    c = Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE) \
      .Append() \
      .Append(cpp_util.GENERATED_FEATURE_MESSAGE %
              cpp_util.ToPosixPath(self._source_file)) \
      .Append() \
      .Append('#include <string>') \
      .Append() \
      .Append('#include "%s.h"' %
              cpp_util.ToPosixPath(self._source_file_filename)) \
      .Append() \
      .Append('#include "base/notreached.h"') \
      .Append() \
      .Concat(cpp_util.OpenNamespace(self._namespace)) \
      .Append()
    )

    # Generate the constructor.
    (c.Append('%s::%s() {' % (self._class_name, self._class_name)) \
      .Sblock()
    )
    for feature in self._feature_defs:
      c.Append('features_["%s"] = %s;' %
               (feature.name, cpp_util.FeatureNameToConstantName(feature.name)))
    (c.Eblock() \
      .Append('}') \
      .Append()
    )

    # Generate the ToString function.
    (c.Append('const char* %s::ToString('
                  '%s::ID id) const {' % (self._class_name, self._class_name)) \
      .Sblock() \
      .Append('switch (id) {') \
      .Sblock()
    )
    for feature in self._feature_defs:
      c.Append('case %s: return "%s";' %
               (cpp_util.FeatureNameToConstantName(feature.name), feature.name))
    (c.Append('case kUnknown: break;') \
      .Append('case kEnumBoundary: break;') \
      .Eblock() \
      .Append('}') \
      .Append('NOTREACHED();') \
    )
    (c.Eblock() \
      .Append('}') \
      .Append()
    )

    # Generate the FromString function.

    (c.Append('%s::ID %s::FromString('
                  'const std::string& id) const {'
                  % (self._class_name, self._class_name)) \
      .Sblock() \
      .Append('const auto& it = features_.find(id);' % self._class_name) \
      .Append('return (it == features_.end()) ? kUnknown : it->second;') \
      .Eblock() \
      .Append('}') \
      .Append() \
      .Cblock(cpp_util.CloseNamespace(self._namespace))
    )

    return c
