// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_UTF_OFFSET_H_
#define UI_BASE_IME_UTF_OFFSET_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {

// Given UTF8 string and its valid offset, returns the offset in UTF16.
// Returns nullopt if the given offset is invalid (not at a valid boundary
// or out of range).
COMPONENT_EXPORT(UI_BASE)
absl::optional<size_t> Utf16OffsetFromUtf8Offset(base::StringPiece text,
                                                 size_t utf8_offset);

// Given UTF16 string and its valid offset, returns the offset in UTF8.
// Returns nullopt if the given offset is invalid (not at a valid boundary
// or out of range).
COMPONENT_EXPORT(UI_BASE)
absl::optional<size_t> Utf8OffsetFromUtf16Offset(base::StringPiece16 text,
                                                 size_t utf16_offset);

}  // namespace ui

#endif  // UI_BASE_IME_UTF_OFFSET_H_
