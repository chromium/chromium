// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_FILE_FORMAT_V2_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_FILE_FORMAT_V2_H_

#include <stdint.h>

namespace caspian {

struct SizeInfo;

bool IsSizeV2(const char* data, unsigned long len);

void ParseSizeInfoV2(const uint8_t* data, unsigned long len, SizeInfo* info);

}  // namespace caspian

#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_FILE_FORMAT_V2_H_
