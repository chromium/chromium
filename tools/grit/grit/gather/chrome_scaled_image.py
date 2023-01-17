# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Gatherer for <structure type="chrome_scaled_image">.
'''


import os
import struct

from grit import exception
from grit import lazy_re
from grit import util
from grit.gather import interface


_PNG_SCALE_CHUNK = b'\0\0\0\0csCl\xc1\x30\x60\x4d'


def _RescaleImage(data, from_scale, to_scale):
  if from_scale != to_scale:
    assert from_scale == 100
    # Rather than rescaling the image we add a custom chunk directing Chrome to
    # rescale it on load. Just append it to the PNG data since
    # _MoveSpecialChunksToFront will move it later anyway.
    data += _PNG_SCALE_CHUNK
  return data


_PNG_MAGIC = b'\x89PNG\r\n\x1a\n'

'''Mandatory first chunk in order for the png to be valid.'''
_FIRST_CHUNK = b'IHDR'

'''Special chunks to move immediately after the IHDR chunk. (so that the PNG
remains valid.)
'''
_SPECIAL_CHUNKS = frozenset(b'csCl npTc'.split())

'''Any ancillary chunk not in this list is deleted from the PNG.'''
_ANCILLARY_CHUNKS_TO_LEAVE = frozenset(
    b'bKGD cHRM gAMA iCCP pHYs sBIT sRGB tRNS acTL fcTL fdAT'.split())


def _MoveSpecialChunksToFront(data):
  '''Move special chunks immediately after the IHDR chunk (so that the PNG
  remains valid). Also delete ancillary chunks that are not on our allowlist.
  '''
  first = [_PNG_MAGIC]
  special_chunks = []
  rest = []
  for chunk in _ChunkifyPNG(data):
    type = chunk[4:8]
    critical = type < b'a'
    if type == _FIRST_CHUNK:
      first.append(chunk)
    elif type in _SPECIAL_CHUNKS:
      special_chunks.append(chunk)
    elif critical or type in _ANCILLARY_CHUNKS_TO_LEAVE:
      rest.append(chunk)
  return b''.join(first + special_chunks + rest)


def _ChunkifyPNG(data):
  '''Given a PNG image, yield its chunks in order.'''
  assert data.startswith(_PNG_MAGIC)
  pos = 8
  while pos != len(data):
    length = 12 + struct.unpack_from('>I', data, pos)[0]
    assert 12 <= length <= len(data) - pos
    yield data[pos:pos+length]
    pos += length


def _MakeBraceGlob(strings):
  '''Given ['foo', 'bar'], return '{foo,bar}', for error reporting.
  '''
  if len(strings) == 1:
    return strings[0]
  else:
    return '{' + ','.join(strings) + '}'


class ChromeScaledImage(interface.GathererBase):
  '''Represents an image that exists in multiple layout variants
  (e.g. "default", "touch") and multiple scale variants
  (e.g. "100_percent", "200_percent").
  '''

  split_context_re_ = lazy_re.compile(r'(.+)_(\d+)_percent\Z')

  def _FindInputFile(self):
    output_context = self.grd_node.GetRoot().output_context
    match = self.split_context_re_.match(output_context)
    if not match:
      raise exception.MissingMandatoryAttribute(
          'All <output> nodes must have an appropriate context attribute'
          ' (e.g. context="touch_200_percent")')
    req_layout, req_scale = match.group(1), int(match.group(2))

    layouts = [req_layout]
    try_default_layout = self.grd_node.GetRoot().fallback_to_default_layout
    if try_default_layout and 'default' not in layouts:
      layouts.append('default')

    scales = [req_scale]
    try_low_res = self.grd_node.FindBooleanAttribute(
        'fallback_to_low_resolution', default=False, skip_self=False)
    if try_low_res and 100 not in scales:
      scales.append(100)

    for layout in layouts:
      for scale in scales:
        dir = '%s_%s_percent' % (layout, scale)
        path = os.path.join(dir, self.rc_file)
        if os.path.exists(self.grd_node.ToRealPath(path)):
          return path, scale, req_scale

    if not try_default_layout:
      # The file was not found in the specified output context and it was
      # explicitly indicated that the default context should not be searched
      # as a fallback, so return an empty path.
      return None, 100, req_scale

    # The file was found in neither the specified context nor the default
    # context, so raise an exception.
    dir = "%s_%s_percent" % (_MakeBraceGlob(layouts),
                             _MakeBraceGlob([str(x) for x in scales]))
    raise exception.FileNotFound(
        'Tried ' + self.grd_node.ToRealPath(os.path.join(dir, self.rc_file)))

  def GetInputPath(self):
    path, scale, req_scale = self._FindInputFile()
    return path

  def Parse(self):
    pass

  def GetTextualIds(self):
    return [self.extkey]

  def GetData(self, lang, encoding):
    assert encoding == util.BINARY

    path, scale, req_scale = self._FindInputFile()
    if path is None:
      return None

    data = util.ReadFile(self.grd_node.ToRealPath(path), util.BINARY)
    data = _RescaleImage(data, scale, req_scale)
    data = _MoveSpecialChunksToFront(data)
    return data

  def Translate(self, *args, **kwargs):
    return self.GetData()
