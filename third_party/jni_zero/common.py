# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common logic needed by other modules."""

import contextlib
import filecmp
import os
import shutil
import tempfile
import pathlib
import zipfile


# Only some methods respect line length, so this is more of a best-effort
# limit.
_TARGET_LINE_LENGTH = 80


class StringBuilder:

  def __init__(self):
    self._sb = []
    self._indent = 0
    self._start_of_line = True

  def __call__(self, value):
    lines = value.splitlines(keepends=True)
    for line in lines:
      if self._start_of_line and line != '\n':
        self._sb.append(' ' * self._indent)
      self._sb.append(line)
      self._start_of_line = line[-1] == '\n'

  def _cur_line_length(self):
    ret = 0
    for l in reversed(self._sb):
      if l.endswith('\n'):
        break
      ret += len(l)
    return ret

  @contextlib.contextmanager
  def _param_list_generator(self):
    values = []
    yield values
    self.param_list(values)

  def param_list(self, values=None):
    if values is None:
      return self._param_list_generator()

    self('(')
    if values:
      punctuation_size = 2 * len(values) # punctuation: ", ()"
      single_line_size = sum(len(v) for v in values) + punctuation_size
      if self._cur_line_length() + single_line_size < _TARGET_LINE_LENGTH:
        self(', '.join(values))
      else:
        self('\n')
        with self.indent(4):
          self(',\n'.join(values))
    self(')')

  def line(self, value=None):
    self(value)
    self('\n')

  @contextlib.contextmanager
  def statement(self):
    yield
    self(';\n')

  @contextlib.contextmanager
  def section(self, section_title):
    self(f'// {section_title}\n')
    yield
    self('\n')

  @contextlib.contextmanager
  def namespace(self, namespace_name):
    if namespace_name is None:
      yield
      return
    value = f' {namespace_name}' if namespace_name else ''
    self(f'namespace{value} {{\n\n')
    yield
    self(f'\n}}  // namespace{value}\n')

  @contextlib.contextmanager
  def block(self, *, indent=2, after=None):
    self(' {\n')
    with self.indent(indent):
      yield
    if after:
      self('}')
      self(after)
      self('\n')
    else:
      self('}\n')

  @contextlib.contextmanager
  def indent(self, amount):
    self._indent += amount
    yield
    self._indent -= amount

  def to_string(self):
    return ''.join(self._sb)


def capitalize(value):
  return value[0].upper() + value[1:]


def escape_class_name(fully_qualified_class):
  """Returns an escaped string concatenating the Java package and class."""
  escaped = fully_qualified_class.replace('_', '_1')
  return escaped.replace('/', '_').replace('$', '_00024')


@contextlib.contextmanager
def atomic_output(path, mode='w+b'):
  with tempfile.NamedTemporaryFile(mode, delete=False) as f:
    try:
      yield f
    finally:
      f.close()

    if not (os.path.exists(path) and filecmp.cmp(f.name, path)):
      pathlib.Path(path).parents[0].mkdir(parents=True, exist_ok=True)
      shutil.move(f.name, path)
    if os.path.exists(f.name):
      os.unlink(f.name)


def add_to_zip_hermetic(zip_file, zip_path, data=None):
  zipinfo = zipfile.ZipInfo(filename=zip_path)
  zipinfo.external_attr = 0o644 << 16
  zipinfo.date_time = (2001, 1, 1, 0, 0, 0)
  zip_file.writestr(zipinfo, data, zipfile.ZIP_STORED)


def should_rename_package(package_name, filter_list_string):
  # If the filter list is empty, all packages should be renamed.
  if not filter_list_string:
    return True

  return any(
      package_name.startswith(pkg_prefix)
      for pkg_prefix in filter_list_string.split(':'))
