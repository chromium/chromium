// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_UTF16_INDEXING_H_
#define UI_GFX_UTF16_INDEXING_H_

#include <stddef.h>

#include <string>

#include "ui/gfx/gfx_export.h"

namespace gfx {

// Returns false if s[index-1] is a high surrogate and s[index] is a low
// surrogate, true otherwise.
GFX_EXPORT bool IsValidCodePointIndex(const std::u16string& s, size_t index);

// |UTF16IndexToOffset| returns the number of code points between |base| and
// |pos| in the given string. |UTF16OffsetToIndex| returns the index that is
// |offset| code points away from the given |base| index. These functions are
// named after glib's |g_utf8_pointer_to_offset| and |g_utf8_offset_to_pointer|,
// which perform the same function for UTF-8. As in glib, it is an error to
// pass an |offset| that walks off the edge of the string.
//
// These functions attempt to deal with invalid use of UTF-16 surrogates in a
// way that makes as much sense as possible: unpaired surrogates are treated as
// single characters, and if an argument index points to the middle of a valid
// surrogate pair, it is treated as though it pointed to the end of that pair.
// The index returned by |UTF16OffsetToIndex| never points to the middle of a
// surrogate pair.
//
// The following identities hold:
//   If |s| contains no surrogate pairs, then
//     UTF16IndexToOffset(s, base, pos) == pos - base
//     UTF16OffsetToIndex(s, base, offset) == base + offset
//   If |pos| does not point to the middle of a surrogate pair, then
//     UTF16OffsetToIndex(s, base, UTF16IndexToOffset(s, base, pos)) == pos
//   Always,
//     UTF16IndexToOffset(s, base, UTF16OffsetToIndex(s, base, ofs)) == ofs
//     UTF16IndexToOffset(s, i, j) == -UTF16IndexToOffset(s, j, i)
GFX_EXPORT ptrdiff_t UTF16IndexToOffset(const std::u16string& s,
                                        size_t base,
                                        size_t pos);
GFX_EXPORT size_t UTF16OffsetToIndex(const std::u16string& s,
                                     size_t base,
                                     ptrdiff_t offset);

}  // namespace gfx

#endif  // UI_GFX_UTF16_INDEXING_H_
