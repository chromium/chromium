// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_DATA_PACK_LITERAL_H_
#define UI_BASE_RESOURCE_DATA_PACK_LITERAL_H_

#include <stddef.h>
#include <stdint.h>

namespace ui {

extern const uint8_t kSamplePakContentsV4[];
extern const size_t kSamplePakSizeV4;
extern const uint8_t kSampleCompressPakContentsV5[];
extern const size_t kSampleCompressPakSizeV5;
extern const uint8_t kSampleCompressScaledPakContents[];
extern const size_t kSampleCompressScaledPakSize;
extern const uint8_t kSamplePakContents2x[];
extern const size_t kSamplePakSize2x;
extern const uint8_t kEmptyPakContents[];
extern const size_t kEmptyPakSize;
extern const uint8_t kSampleCorruptPakContents[];
extern const size_t kSampleCorruptPakSize;
extern const uint8_t kSampleMisorderedPakContents[];
extern const size_t kSampleMisorderedPakSize;

}  // namespace ui

#endif  // UI_BASE_RESOURCE_DATA_PACK_LITERAL_H_
