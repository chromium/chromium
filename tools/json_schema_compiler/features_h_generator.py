# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from code_util import Code
import cpp_util


class HGenerator(object):

  def Generate(self, features, source_file, namespace):
    return _Generator(features, source_file, namespace).Generate()


class _Generator(object):
  """A .cc generator for features.
  """

  def __init__(self, features, source_file, namespace):
    self._feature_defs = features
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
      .Append()
    )

    # Hack: for the purpose of gyp the header file will always be the source
    # file with its file extension replaced by '.h'. Assume so.
    output_file = os.path.splitext(self._namespace.source_file)[0] + '.h'
    ifndef_name = cpp_util.GenerateIfndefName(output_file)

    (c.Append('#ifndef %s' % ifndef_name) \
      .Append('#define %s' % ifndef_name) \
      .Append()
    )

    (c.Append('#include <map>') \
      .Append('#include <string>') \
      .Append() \
      .Concat(cpp_util.OpenNamespace(self._namespace)) \
      .Append()
    )

    (c.Append('class %s {' % self._class_name) \
      .Append(' public:') \
      .Sblock() \
      .Concat(self._GeneratePublicBody()) \
      .Eblock() \
      .Append(' private:') \
      .Sblock() \
      .Concat(self._GeneratePrivateBody()) \
      .Eblock('};') \
      .Append() \
      .Cblock(cpp_util.CloseNamespace(self._namespace))
    )
    (c.Append('#endif  // %s' % ifndef_name) \
      .Append()
    )
    return c

  def _GeneratePublicBody(self):
    c = Code()

    (c.Append('%s();' % self._class_name) \
      .Append() \
      .Append('enum ID {') \
      .Concat(self._GenerateEnumConstants()) \
      .Eblock('};') \
      .Append() \
      .Append('const char* ToString(ID id) const;') \
      .Append('ID FromString(const std::string& id) const;') \
      .Append()
    )
    return c

  def _GeneratePrivateBody(self):
    return Code().Append('std::map<std::string, '
                         '%s::ID> features_;' % self._class_name)

  def _GenerateEnumConstants(self):
    c = Code()

    (c.Sblock() \
      .Append('kUnknown,')
    )
    for feature in self._feature_defs:
      c.Append('%s,' % cpp_util.FeatureNameToConstantName(feature.name))
    c.Append('kEnumBoundary')

    return c
