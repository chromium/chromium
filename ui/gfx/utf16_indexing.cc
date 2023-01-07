// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/utf16_indexing.h"

#include "base/check_op.h"
#include "base/third_party/icu/icu_utf.h"

namespace gfx {

bool IsValidCodePointIndex(const std::u16string& s, size_t index) {
  return index == 0 || index == s.length() ||
    !(CBU16_IS_TRAIL(s[index]) && CBU16_IS_LEAD(s[index - 1]));
}

ptrdiff_t UTF16IndexToOffset(const std::u16string& s, size_t base, size_t pos) {
  // The indices point between UTF-16 words (range 0 to s.length() inclusive).
  // In order to consistently handle indices that point to the middle of a
  // surrogate pair, we count the first word in that surrogate pair and not
  // the second. The test "s[i] is not the second half of a surrogate pair" is
  // "IsValidCodePointIndex(s, i)".
  DCHECK_LE(base, s.length());
  DCHECK_LE(pos, s.length());
  ptrdiff_t delta = 0;
  while (base < pos)
    delta += IsValidCodePointIndex(s, base++) ? 1 : 0;
  while (pos < base)
    delta -= IsValidCodePointIndex(s, pos++) ? 1 : 0;
  return delta;
}

size_t UTF16OffsetToIndex(const std::u16string& s,
                          size_t base,
                          ptrdiff_t offset) {
  DCHECK_LE(base, s.length());
  // As in UTF16IndexToOffset, we count the first half of a surrogate pair, not
  // the second. When stepping from pos to pos+1 we check s[pos:pos+1] == s[pos]
  // (Python syntax), hence pos++. When stepping from pos to pos-1 we check
  // s[pos-1], hence --pos.
  size_t pos = base;
  while (offset > 0 && pos < s.length())
    offset -= IsValidCodePointIndex(s, pos++) ? 1 : 0;
  while (offset < 0 && pos > 0)
    offset += IsValidCodePointIndex(s, --pos) ? 1 : 0;
  // If offset != 0 then we ran off the edge of the string, which is a contract
  // violation but is handled anyway (by clamping) in release for safety.
  DCHECK_EQ(offset, 0);
  // Since the second half of a surrogate pair has "length" zero, there is an
  // ambiguity in the returned position. Resolve it by always returning a valid
  // index.
  if (!IsValidCodePointIndex(s, pos))
    ++pos;
  return pos;
}

}  // namespace gfx
