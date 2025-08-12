// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_METADATA_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_METADATA_H_

#include <cstddef>  // for size_t
#include <optional>

#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"

namespace ui {

// Struct that holds metadata for data being copied or pasted that is relevant
// to evaluating enterprise policies.
struct ClipboardMetadata {
  // Size of the clipboard data. null when files are copied, or sometimes when
  // created from Android JNI.
  // TODO(crbug.com/344593255): Ensure that Android JNI consistently passes in
  //  non-null size.
  std::optional<size_t> size;

  // Format type of clipboard data.
  ClipboardFormatType format_type;

  // Sequence number of the clipboard interaction.
  ClipboardSequenceNumberToken seqno;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_METADATA_H_
