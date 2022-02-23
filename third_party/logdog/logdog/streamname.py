# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import collections
import re
import string

# third_party/
from six.moves import urllib

_STREAM_SEP = '/'
_ALNUM_CHARS = string.ascii_letters + string.digits
_VALID_SEG_CHARS = _ALNUM_CHARS + ':_-.'
_SEGMENT_RE_BASE = r'[a-zA-Z0-9][a-zA-Z0-9:_\-.]*'
_SEGMENT_RE = re.compile('^' + _SEGMENT_RE_BASE + '$')
_STREAM_NAME_RE = re.compile('^(' + _SEGMENT_RE_BASE + ')(/' +
                             _SEGMENT_RE_BASE + ')*$')
_MAX_STREAM_NAME_LENGTH = 4096

_MAX_TAG_KEY_LENGTH = 64
_MAX_TAG_VALUE_LENGTH = 4096


def validate_stream_name(v, maxlen=None):
  """Verifies that a given stream name is valid.

  Args:
    v (str): The stream name string.


  Raises:
    ValueError if the stream name is invalid.
  """
  maxlen = maxlen or _MAX_STREAM_NAME_LENGTH
  if len(v) > maxlen:
    raise ValueError('Maximum length exceeded (%d > %d)' % (len(v), maxlen))
  if _STREAM_NAME_RE.match(v) is None:
    raise ValueError('Invalid stream name: %r' % v)


def validate_tag(key, value):
  """Verifies that a given tag key/value is valid.

  Args:
    k (str): The tag key.
    v (str): The tag value.

  Raises:
    ValueError if the tag is not valid.
  """
  validate_stream_name(key, maxlen=_MAX_TAG_KEY_LENGTH)
  validate_stream_name(value, maxlen=_MAX_TAG_VALUE_LENGTH)


def normalize_segment(seg, prefix=None):
  """Given a string, mutate it into a valid segment name.

  This operates by replacing invalid segment name characters with underscores
  (_) when encountered.

  A special case is when "seg" begins with non-alphanumeric character. In this
  case, we will prefix it with the "prefix", if one is supplied. Otherwise,
  raises ValueError.

  See _VALID_SEG_CHARS for all valid characters for a segment.

  Raises:
    ValueError: If normalization could not be successfully performed.
  """
  if not seg:
    if prefix is None:
      raise ValueError('Cannot normalize empty segment with no prefix.')
    seg = prefix
  else:

    def replace_if_invalid(ch, first=False):
      ret = ch if ch in _VALID_SEG_CHARS else '_'
      if first and ch not in _ALNUM_CHARS:
        if prefix is None:
          raise ValueError('Segment has invalid beginning, and no prefix was '
                           'provided.')
        return prefix + ret
      return ret

    seg = ''.join(replace_if_invalid(ch, i == 0) for i, ch in enumerate(seg))

  if _SEGMENT_RE.match(seg) is None:
    raise AssertionError('Normalized segment is still invalid: %r' % seg)

  return seg


def normalize(v, prefix=None):
  """Given a string, mutate it into a valid stream name.

  This operates by replacing invalid stream name characters with underscores (_)
  when encountered.

  A special case is when any segment of "v" begins with an non-alphanumeric
  character. In this case, we will prefix the segment with the "prefix", if one
  is supplied. Otherwise, raises ValueError.

  See _STREAM_NAME_RE for a description of a valid stream name.

  Raises:
    ValueError: If normalization could not be successfully performed.
  """
  normalized = _STREAM_SEP.join(
      normalize_segment(seg, prefix=prefix) for seg in v.split(_STREAM_SEP))
  # Validate the resulting string.
  validate_stream_name(normalized)
  return normalized


class StreamPath(collections.namedtuple('_StreamPath', ('prefix', 'name'))):
  """StreamPath is a full stream path.

  This consists of both a stream prefix and a stream name.

  When constructed with parse or make, the stream path must be completely valid.
  However, invalid stream paths may be constructed by manually instantiation.
  This can be useful for wildcard query values (e.g., "prefix='foo/*/bar/**'").
  """

  @classmethod
  def make(cls, prefix, name):
    """Returns (StreamPath): The validated StreamPath instance.

    Args:
      prefix (str): the prefix component
      name (str): the name component

    Raises:
      ValueError: If path is not a full, valid stream path string.
    """
    inst = cls(prefix=prefix, name=name)
    inst.validate()
    return inst

  @classmethod
  def parse(cls, path):
    """Returns (StreamPath): The parsed StreamPath instance.

    Args:
      path (str): the full stream path to parse.

    Raises:
      ValueError: If path is not a full, valid stream path string.
    """
    parts = path.split('/+/', 1)
    if len(parts) != 2:
      raise ValueError('Not a full stream path: [%s]' % (path,))
    return cls.make(*parts)

  def validate(self):
    """Raises: ValueError if this is not a valid stream name."""
    try:
      validate_stream_name(self.prefix)
    except ValueError as e:
      raise ValueError('Invalid prefix component [%s]: %s' % (self.prefix, e,))

    try:
      validate_stream_name(self.name)
    except ValueError as e:
      raise ValueError('Invalid name component [%s]: %s' % (self.name, e,))

  def __str__(self):
    return '%s/+/%s' % (self.prefix, self.name)


def get_logdog_viewer_url(host, project, *stream_paths):
  """Returns (str): The LogDog viewer URL for the named stream(s).

  Args:
    host (str): The name of the Coordiantor host.
    project (str): The project name.
    stream_paths: A set of StreamPath instances for the stream paths to
        generate the URL for.
  """
  return urllib.parse.urlunparse((
      'https',  # Scheme
      host,  # netloc
      'v/',  # path
      '',  # params
      '&'.join(('s=%s' % (urllib.parse.quote('%s/%s' % (project, path), ''))
                for path in stream_paths)),  # query
      '',  # fragment
  ))
