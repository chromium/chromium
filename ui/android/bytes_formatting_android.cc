// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/text/bytes_formatting.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/byte_size.h"
#include "base/numerics/safe_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/BytesFormatting_jni.h"

namespace ui {

static std::u16string JNI_BytesFormatting_FormatSpeed(int64_t speed) {
  return FormatSpeed(base::ByteSize(base::checked_cast<uint64_t>(speed)));
}

}  // namespace ui

DEFINE_JNI(BytesFormatting)
