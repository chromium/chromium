# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilies for the processing of schema python structures.
"""


def CapitalizeFirstLetter(value):
  return value[0].capitalize() + value[1:]


def GetNamespace(ref):
  return SplitNamespace(ref)[0]


def StripNamespace(ref):
  return SplitNamespace(ref)[1]


def SplitNamespace(ref):
  """Returns (namespace, entity) from |ref|, e.g. app.window.AppWindow ->
  (app.window, AppWindow). If |ref| isn't qualified then returns (None, ref).
  """
  if '.' in ref:
    return tuple(ref.rsplit('.', 1))
  return (None, ref)


def JsFunctionNameToClassName(namespace_name, function_name):
  """Transform a fully qualified function name like foo.bar.baz into FooBarBaz

  Also strips any leading 'Experimental' prefix."""
  parts = []
  full_name = namespace_name + "." + function_name
  for part in full_name.split("."):
    parts.append(CapitalizeFirstLetter(part))
  if parts[0] == "Experimental":
    del parts[0]
  class_name = "".join(parts)
  return class_name
