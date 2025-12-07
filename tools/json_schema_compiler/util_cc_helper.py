# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_API_UTIL_NAMESPACE = 'json_schema_compiler::util'


class UtilCCHelper(object):
  """A util class that generates code that uses
  tools/json_schema_compiler/util.cc.
  """

  def __init__(self, type_manager):
    self._type_manager = type_manager

  def PopulateArrayFromListFunction(self, optional):
    """Returns the function to turn a list into a vector.
    """
    populate_list_fn = ('PopulateOptionalArrayFromList'
                        if optional else 'PopulateArrayFromList')
    return ('%s::%s') % (_API_UTIL_NAMESPACE, populate_list_fn)

  def CreateValueFromArray(self, src):
    """Generates code to create a scoped_pt<Value> from the array at src.

    |src| The variable to convert, either a vector or std::unique_ptr<vector>.
    """
    return '%s::CreateValueFromArray(%s)' % (_API_UTIL_NAMESPACE, src)

  def AppendToContainer(self, container, value):
    """Appends |value| to |container|.
    """
    return '%s::AppendToContainer(%s, %s);' % (_API_UTIL_NAMESPACE, container,
                                               value)

  def GetIncludePath(self):
    return '#include "tools/json_schema_compiler/util.h"'

  def GetValueTypeString(self, value):
    return 'UTF8ToUTF16(base::Value::GetTypeName(%s.type()))' % value
