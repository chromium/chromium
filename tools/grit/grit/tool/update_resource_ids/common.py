# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def AlignUp(v, align):
  return (v + align - 1) // align * align


def StripPlural(s):
  assert s.endswith('s'), 'Expect %s to be plural' % s
  return s[:-1]


class Color:

  def _MakeColor(code):
    t = '\033[' + code + 'm%s\033[0m'
    return lambda s: t % s

  NONE = staticmethod(lambda s: s)
  RED = staticmethod(_MakeColor('31'))
  GREEN = staticmethod(_MakeColor('32'))
  YELLOW = staticmethod(_MakeColor('33'))
  BLUE = staticmethod(_MakeColor('34'))
  MAGENTA = staticmethod(_MakeColor('35'))
  CYAN = staticmethod(_MakeColor('36'))
  WHITE = staticmethod(_MakeColor('37'))
  GRAY = staticmethod(_MakeColor('30;1'))


class TagInfo:
  """Stores resource_ids tag entry (e.g., {"includes": 100} pair)."""

  def __init__(self, raw_key, raw_value):
    """TagInfo Constructor.

    Args:
      raw_key: parser.AnnotatedValue for the parsed key, e.g., "includes".
      raw_value: parser.AnnotatedValue for the parsed value, e.g., 100.
    """
    # Tag name, e.g., 'include' (no "s" at end).
    self.name = StripPlural(raw_key.val)
    # |len(raw_value) > 1| is possible, e.g., see grd_reader_unittest.py's
    # testAssignFirstIdsMultipleMessages. This feature seems unused though.
    # TODO(huangs): Reconcile this (may end up removing multi-value feature).
    assert len(raw_value) == 1
    # Inclusive start *position* of the tag's start ID in resource_ids.
    self.lo = raw_value[0].lo
    # Exclusive end *position* of the tag's start ID in resource_ids.
    self.hi = raw_value[0].hi
    # The tag's start ID. Initially the old value, but may be reassigned to new.
    self.id = raw_value[0].val
    # The number of IDs the tag uses, to be assigned by ItemInfo.SetUsages().
    self.usage = None


class ItemInfo:
  """resource_ids item, containing multiple TagInfo."""

  def __init__(self, lo, grd, raw_item):
    # Inclusive start position of the item's key. Serve as unique identifier.
    self.lo = lo
    # The GRD filename for the item.
    self.grd = grd
    # Optional META information for the item.
    self.meta = None
    # List of TagInfo associated witih the item.
    self.tags = []
    for k, v in raw_item.items():
      if k.val == 'META':
        assert self.meta is None
        self.meta = v  # Not flattened.
      else:
        self.tags.append(TagInfo(k, v))
    self.tags.sort(key=lambda tag: tag.lo)

  def SetUsages(self, tag_name_to_usage):
    for tag in self.tags:
      tag.usage = tag_name_to_usage.get(tag.name, 0)


def BuildItemList(root_obj):
  """Extracts ID assignments and structure from parsed resource_ids.

  Returns: A list of ItemInfo, ordered by |lo|.
  """
  item_list = []
  grd_seen = set()
  for raw_key, raw_item in root_obj.items():  # Unordered.
    grd = raw_key.val
    if grd == 'SRCDIR':
      continue
    if not grd.endswith('.grd'):
      raise ValueError('Invalid GRD file: %s' % grd)
    if grd in grd_seen:
      raise ValueError('Duplicate GRD: %s' % grd)
    grd_seen.add(grd)
    item_list.append(ItemInfo(raw_key.lo, grd, raw_item))
  item_list.sort(key=lambda item: item.lo)
  return item_list
