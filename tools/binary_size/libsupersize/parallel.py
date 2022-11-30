# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers related to multiprocessing."""

import builtins
import itertools
import logging
import multiprocessing
import multiprocessing.dummy
import os
import sys
import threading
import traceback

from multiprocessing import process

DISABLE_ASYNC = os.environ.get('SUPERSIZE_DISABLE_ASYNC') == '1'
if DISABLE_ASYNC:
  logging.warning('Running in synchronous mode.')

_is_child_process = False
_silence_exceptions = False

# Used to pass parameters to forked processes without pickling.
_fork_params = None
_fork_kwargs = None


# Avoid printing backtrace for every worker for Ctrl-C.
def _PatchMultiprocessing():
  old_run = process.BaseProcess.run

  def new_run(self):
    try:
      return old_run(self)
    except (BrokenPipeError, KeyboardInterrupt):
      sys.exit(1)

  process.BaseProcess.run = new_run


_PatchMultiprocessing()


class _ImmediateResult:
  def __init__(self, value):
    self._value = value

  def get(self):
    return self._value

  def wait(self):
    pass

  def ready(self):
    return True

  def successful(self):
    return True


class _ExceptionWrapper:
  """Used to marshal exception messages back to main process."""

  def __init__(self, msg, exception_type=None):
    self.msg = msg
    self.exception_type = exception_type

  def MaybeThrow(self):
    if self.exception_type:
      raise getattr(builtins,
                    self.exception_type)('Originally caused by: ' + self.msg)


class _FuncWrapper:
  """Runs on the fork()'ed side to catch exceptions and spread *args."""

  def __init__(self, func):
    global _is_child_process
    _is_child_process = True
    self._func = func

  def __call__(self, index, _=None):
    try:
      return self._func(*_fork_params[index], **dict(_fork_kwargs))
    except BaseException as e:
      # Only keep the exception type for builtin exception types or else risk
      # further marshalling exceptions.
      exception_type = None
      if type(e).__name__ in dir(builtins):
        exception_type = type(e).__name__
      # multiprocessing is supposed to catch and return exceptions automatically
      # but it doesn't seem to work properly :(.
      return _ExceptionWrapper(traceback.format_exc(), exception_type)


class _WrappedResult:
  """Allows for host-side logic to be run after child process has terminated.

  * Raises exception caught by _FuncWrapper.
  * Allows for custom unmarshalling of return value.
  """

  def __init__(self, result, decode_func=None):
    self._result = result
    self._decode_func = decode_func

  def get(self):
    self.wait()
    value = self._result.get()
    _CheckForException(value)
    if not self._decode_func or not self._result.successful():
      return value
    return self._decode_func(value)

  def wait(self):
    self._result.wait()

  def ready(self):
    return self._result.ready()

  def successful(self):
    return self._result.successful()


def _CheckForException(value):
  if isinstance(value, _ExceptionWrapper):
    global _silence_exceptions
    if not _silence_exceptions:
      value.MaybeThrow()
      _silence_exceptions = True
      logging.error('Subprocess raised an exception:\n%s', value.msg)
    sys.exit(1)


def _MakeProcessPool(job_params, **job_kwargs):
  global _fork_params
  global _fork_kwargs
  assert _fork_params is None
  assert _fork_kwargs is None
  pool_size = min(len(job_params), multiprocessing.cpu_count())
  _fork_params = job_params
  _fork_kwargs = job_kwargs
  ret = multiprocessing.Pool(pool_size)
  _fork_params = None
  _fork_kwargs = None
  return ret


def ForkAndCall(func, args, decode_func=None):
  """Runs |func| in a fork'ed process.

  Returns:
    A Result object (call .get() to get the return value)
  """
  if DISABLE_ASYNC:
    result = _ImmediateResult(func(*args))
  else:
    pool = _MakeProcessPool([args])  # Omit |kwargs|.
    result = pool.apply_async(_FuncWrapper(func), (0, ))
    pool.close()
  return _WrappedResult(result, decode_func=decode_func)


def BulkForkAndCall(func, arg_tuples, **kwargs):
  """Calls |func| in a fork'ed process for each set of args within |arg_tuples|.

  Args:
    kwargs: Common keyword arguments to be passed to |func|.

  Yields the return values as they come in.
  """
  arg_tuples = list(arg_tuples)
  if not arg_tuples:
    return

  if DISABLE_ASYNC:
    for args in arg_tuples:
      yield func(*args, **kwargs)
    return

  pool = _MakeProcessPool(arg_tuples, **kwargs)
  wrapped_func = _FuncWrapper(func)
  try:
    for result in pool.imap_unordered(wrapped_func, range(len(arg_tuples))):
      _CheckForException(result)
      yield result
  finally:
    pool.close()
    pool.join()


def CallOnThread(func, *args, **kwargs):
  """Calls |func| on a new thread and returns a promise for its return value."""
  if DISABLE_ASYNC:
    return _ImmediateResult(func(*args, **kwargs))
  pool = multiprocessing.dummy.Pool(1)
  result = pool.apply_async(func, args=args, kwds=kwargs)
  pool.close()
  return result


def EncodeDictOfLists(d, key_transform=None, value_transform=None):
  """Serializes a dict where values are lists of strings.

  Does not support '' as keys, nor [''] as values.
  """
  assert '' not in d
  assert [''] not in iter(d.values())
  keys = iter(d)
  if key_transform:
    keys = (key_transform(k) for k in keys)
  keys = '\x01'.join(keys)
  if value_transform:
    values = '\x01'.join(
        '\x02'.join(value_transform(y) for y in x) for x in d.values())
  else:
    values = '\x01'.join('\x02'.join(x) for x in d.values())
  return keys, values


def JoinEncodedDictOfLists(encoded_values):
  assert isinstance(encoded_values, list), 'Does not work with generators'
  return ('\x01'.join(x[0] for x in encoded_values if x[0]),
          '\x01'.join(x[1] for x in encoded_values if x[1]))


def DecodeDictOfLists(encoded_keys_and_values,
                      key_transform=None,
                      value_transform=None):
  """Deserializes a dict where values are lists of strings."""
  encoded_keys, encoded_values = encoded_keys_and_values
  if not encoded_keys:
    return {}
  keys = encoded_keys.split('\x01')
  if key_transform:
    keys = (key_transform(k) for k in keys)
  encoded_lists = encoded_values.split('\x01')
  ret = {}
  for key, encoded_list in zip(keys, encoded_lists):
    if not encoded_list:
      values = []
    else:
      values = encoded_list.split('\x02')
      if value_transform:
        for i in range(len(values)):
          values[i] = value_transform(values[i])
    ret[key] = values
  return ret


EMPTY_ENCODED_DICT = EncodeDictOfLists({})
