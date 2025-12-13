# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common logic needed by other modules."""

import contextlib
import dataclasses
import filecmp
import os
import shutil
import tempfile
import pathlib
import zipfile


# Only some methods respect line length, so this is more of a best-effort
# limit.
_TARGET_LINE_LENGTH = 100


@dataclasses.dataclass(frozen=True)
class JniMode:
  is_hashing: bool = False
  is_muxing: bool = False
  is_per_file: bool = False


JniMode.MUXING = JniMode(is_muxing=True)


class StringBuilder:

  def __init__(self):
    self._sb = []
    self._indent = 0
    self._comment_indent = None
    self._in_cpp_macro = False
    self._cur_line_len = 0

  def __call__(self, value):
    lines = value.splitlines(keepends=True)
    for line in lines:
      # Add any applicable prefix.
      if self._cur_line_len == 0:
        if self._comment_indent is not None:
          self._sb.append(' ' * self._comment_indent)
          # Do not add trailing whitespace for blank lines.
          prefix = '//' if line == '\n' else '// '
          self._sb.append(prefix)
          self._cur_line_len += self._comment_indent + len(prefix)

        if line != '\n' and self._indent > 0:
          self._sb.append(' ' * self._indent)
          self._cur_line_len += self._indent

      self._sb.append(line)
      self._cur_line_len += len(line)

      if line[-1] == '\n':
        if self._in_cpp_macro:
          self._sb[-1] = self._sb[-1][:-1]
          remaining = _TARGET_LINE_LENGTH - self._cur_line_len - 1
          if remaining > 0:
            self._sb.append(' ' * remaining)
          self._sb.append(' \\\n')

        self._cur_line_len = 0

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
      if self._cur_line_len + single_line_size < _TARGET_LINE_LENGTH:
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
    assert not self._in_cpp_macro
    if not ''.join(self._sb[-2:]).endswith('\n\n'):
      self('\n')
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
    if self._in_cpp_macro:
      self(f'\n}}  /* namespace{value} */\n')
    else:
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

  @contextlib.contextmanager
  def commented_section(self):
    assert self._comment_indent is None
    assert not self._in_cpp_macro
    self._comment_indent = self._indent
    self._indent = 0
    yield
    self._indent = self._comment_indent
    self._comment_indent = None

  @contextlib.contextmanager
  def cpp_macro(self, macro_name):
    assert self._indent == 0
    assert not self._in_cpp_macro
    self(f'#define {macro_name}()')
    self._in_cpp_macro = True
    self('\n')
    with self.indent(2):
      yield
    self._in_cpp_macro = False
    # Check that the last call to __call__ ended with a \n, which will result
    # in self._sb ending with [indent, slash-and-newline].
    assert self._cur_line_len == 0
    assert self._sb[-1] == ' \\\n', 'was: ' + self._sb[-1]
    assert self._sb[-2].isspace(), 'was: ' + self._sb[-2]
    self._sb.pop()
    self._sb[-1] = '\n'

  def to_string(self):
    return ''.join(self._sb)


def capitalize(value):
  return value[0].upper() + value[1:]


def jni_mangle(name):
  """Performs JNI mangling on the given name."""
  # https://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/design.html#wp615
  return name.replace('_', '_1').replace('/', '_').replace('$', '_00024')


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


def should_prefix_package(package_name, filter_list_string):
  # Never prefix system packages.
  if package_name.startswith(('android.', 'java.')):
    return False
  # If the filter list is empty, all packages should be prefixed.
  if not filter_list_string:
    return True

  return any(
      package_name.startswith(pkg_prefix)
      for pkg_prefix in filter_list_string.split(':'))
